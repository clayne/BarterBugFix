extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

class BarterFixHook
{
public:
	static void Hook()
	{
		struct Code : Xbyak::CodeGenerator
		{
			Code(uintptr_t func_addr)
			{
				// r13 == cur_IED
				// r15 == new_IED
				// cont_handle == [rbp+57h+var_9C]

				mov(rdx, r13);
				mov(r8, r15);
				mov(r9, ptr[rbp - 0x49]);
				mov(rax, func_addr);
				jmp(rax);
			}
		} xbyakCode{ uintptr_t(distribute) };

		_SetCount_14010E5701 = FenixUtils::add_trampoline<5, 15895, 0xb7e, true>(&xbyakCode);  // SkyrimSE.exe+1EB5AE
		_SetCount_14010E5701 = FenixUtils::add_trampoline<5, 15895, 0xc7e, true>(&xbyakCode);  // SkyrimSE.exe+1EB6AE
	}

private:
	static void distribute(RE::ExtraDataList* new_EDL, RE::InventoryEntryData* cur_IED,
		[[maybe_unused]] RE::InventoryEntryData* new_IED, uint32_t cont_handle)
	{
		auto count = cur_IED->countDelta;
		while (count >= INT16_MAX || count <= INT16_MIN) {
			int32_t cur_count = count > 0 ? 30000 : -30000;
			
			auto new_EDL_new = new RE::ExtraDataList();
			_generic_foo_<11534, void(RE::ExtraDataList * list, uint32_t * handle)>::eval(new_EDL_new, &cont_handle);
			_SetCount_14010E5701(new_EDL_new, cur_count);
			new_IED->AddExtraList(new_EDL_new);

			count -= cur_count;
		}

		if (count) {
			_SetCount_14010E5701(new_EDL, count);
		}
	}

	static inline REL::Relocation<void(RE::ExtraDataList* a1, int16_t count)> _SetCount_14010E5701;
	static inline REL::Relocation<void(RE::ExtraDataList* a1, int16_t count)> _SetCount_14010E5702;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		BarterFixHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
