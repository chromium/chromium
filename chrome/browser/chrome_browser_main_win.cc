// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/chrome_browser_main_win.h"

// windows.h must be included before shellapi.h
#include <windows.h>

#include <delayimp.h>
#include <shellapi.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/dcheck_is_on.h"
#include "base/enterprise_util.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer_cleaner.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/base_tracing.h"
#include "base/version.h"
#include "base/win/pe_image.h"
#include "base/win/win_util.h"
#include "base/win/wrapped_window_proc.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/active_use_util.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/shell_integration_win.h"
#include "chrome/browser/ui/accessibility_util.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/last_browser_file_util.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log_reporter.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_update.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/win/browser_util.h"
#include "chrome/browser/win/chrome_elf_init.h"
#include "chrome/browser/win/conflicts/enumerate_input_method_editors.h"
#include "chrome/browser/win/conflicts/enumerate_shell_extensions.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/module_event_sink_impl.h"
#include "chrome/browser/win/remove_app_compat_entries.h"
#include "chrome/browser/win/util_win_service.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crash/core/app/crash_export_thunks.h"
#include "components/crash/core/app/dump_hung_process_with_ptype.h"
#include "components/crash/core/common/crash_key.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "sandbox/policy/switches.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source_win.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/win/hidden_window.h"
#include "ui/base/win/message_box_win.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/system_fonts_win.h"
#include "ui/gfx/win/crash_id_helper.h"
#include "ui/strings/grit/app_locale_settings.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#endif

namespace {

typedef HRESULT (STDAPICALLTYPE* RegisterApplicationRestartProc)(
    const wchar_t* command_line,
    DWORD flags);

void InitializeWindowProcExceptions() {
  base::win::WinProcExceptionFilter exception_filter =
      base::win::SetWinProcExceptionFilter(&CrashForException_ExportThunk);
  DCHECK(!exception_filter);
}

// TODO(siggi): Remove once https://crbug.com/806661 is resolved.
void DumpHungRendererProcessImpl(const base::Process& renderer) {
  // Use a distinguishing process type for these reports.
  crash_reporter::DumpHungProcessWithPtype(renderer, "hung-renderer");
}

int GetMinimumFontSize() {
  int min_font_size;
  base::StringToInt(l10n_util::GetStringUTF16(IDS_MINIMUM_UI_FONT_SIZE),
                    &min_font_size);
  return min_font_size;
}

class TranslationDelegate : public installer::TranslationDelegate {
 public:
  std::wstring GetLocalizedString(int installer_string_id) override;
};

void DelayedRecordProcessorMetrics() {
  mojo::Remote<chrome::mojom::ProcessorMetrics> remote_util_win =
      LaunchProcessorMetricsService();
  auto* remote_util_win_ptr = remote_util_win.get();
  remote_util_win_ptr->RecordProcessorMetrics(
      base::DoNothingWithBoundArgs(std::move(remote_util_win)));
}

// Initializes the ModuleDatabase on its owning sequence. Also starts the
// enumeration of registered modules in the Windows Registry.
void InitializeModuleDatabase(
    bool is_third_party_blocking_policy_enabled) {
  DCHECK(ModuleDatabase::GetTaskRunner()->RunsTasksInCurrentSequence());

  ModuleDatabase::SetInstance(
      std::make_unique<ModuleDatabase>(is_third_party_blocking_policy_enabled));

  auto* module_database = ModuleDatabase::GetInstance();
  module_database->StartDrainingModuleLoadAttemptsLog();

  // Enumerate shell extensions and input method editors. It is safe to use
  // base::Unretained() here because the ModuleDatabase is never freed.
  EnumerateShellExtensions(
      base::BindRepeating(&ModuleDatabase::OnShellExtensionEnumerated,
                          base::Unretained(module_database)),
      base::BindOnce(&ModuleDatabase::OnShellExtensionEnumerationFinished,
                     base::Unretained(module_database)));
  EnumerateInputMethodEditors(
      base::BindRepeating(&ModuleDatabase::OnImeEnumerated,
                          base::Unretained(module_database)),
      base::BindOnce(&ModuleDatabase::OnImeEnumerationFinished,
                     base::Unretained(module_database)));
}

// Notes on the OnModuleEvent() callback.
//
// The ModuleDatabase uses the TimeDateStamp value of the DLL to uniquely
// identify modules as they are discovered. Unlike the SizeOfImage, this value
// isn't provided via LdrDllNotification events or CreateToolhelp32Snapshot().
//
// The easiest way to obtain the TimeDateStamp is to read the mapped module in
// memory. Unfortunately, this could cause an ACCESS_VIOLATION_EXCEPTION if the
// module is unloaded before being accessed. This can occur when enumerating
// already loaded modules with CreateToolhelp32Snapshot(). Note that this
// problem doesn't affect LdrDllNotification events, where it is guaranteed that
// the module stays in memory for the duration of the callback.
//
// To get around this, there are multiple solutions:
// (1) Read the file on disk instead.
//     Sidesteps the problem altogether. The drawback is that it must be done on
//     a sequence that allow blocking IO, and it is way slower. We don't want to
//     pay that price for each module in the process. This can fail if the file
//     can not be found when attemping to read it.
//
// (2) Increase the reference count of the module.
//     Calling ::LoadLibraryEx() or ::GetModuleHandleEx() lets us ensure that
//     the module won't go away while we hold the extra handle. It's still
//     possible that the module was unloaded before we have a chance to increase
//     the reference count, which would mean either reloading the DLL, or
//     failing to get a new handle.
//
//     This isn't ideal but the worst that can happen is that we hold the last
//     reference to the module. The DLL would be unloaded on our thread when
//     ::FreeLibrary() is called. This could go horribly wrong if the DLL's
//     creator didn't consider this possibility.
//
// (3) Do it in a Structured Exception Handler (SEH)
//     Make the read inside a __try/__except handler and handle the possible
//     ACCESS_VIOLATION_EXCEPTION if it happens.
//
// The current solution is (3) with a fallback that uses (1). In the rare case
// that both fail to get the TimeDateStamp, the module load event is dropped
// altogether, as our best effort was unsuccessful.

// Gets the TimeDateStamp from the file on disk and, if successful, sends the
// load event to the ModuleDatabase.
void HandleModuleLoadEventWithoutTimeDateStamp(
    const base::FilePath& module_path,
    size_t module_size) {
  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  bool got_time_date_stamp = GetModuleImageSizeAndTimeDateStamp(
      module_path, &size_of_image, &time_date_stamp);

  // Simple sanity check.
  got_time_date_stamp = got_time_date_stamp && size_of_image == module_size;

  // Drop the load event if it's not possible to get the time date stamp.
  if (!got_time_date_stamp)
    return;

  ModuleDatabase::HandleModuleLoadEvent(
      content::PROCESS_TYPE_BROWSER, module_path, module_size, time_date_stamp);
}

// Helper function for getting the module size associated with a module in this
// process based on its load address.
uint32_t GetModuleSizeOfImage(const void* module_load_address) {
  base::win::PEImage pe_image(module_load_address);
  return pe_image.GetNTHeaders()->OptionalHeader.SizeOfImage;
}

// Helper function for getting the time date stamp associated with a module in
// this process based on its load address.
uint32_t GetModuleTimeDateStamp(const void* module_load_address) {
  base::win::PEImage pe_image(module_load_address);
  return pe_image.GetNTHeaders()->FileHeader.TimeDateStamp;
}

// An exception filter for handling Access Violation exceptions within the
// memory range [module_load_address, module_load_address + size_of_image).
DWORD FilterAccessViolation(DWORD exception_code,
                            const EXCEPTION_POINTERS* exception_information,
                            void* module_load_address,
                            uint32_t size_of_image) {
  if (exception_code != EXCEPTION_ACCESS_VIOLATION)
    return EXCEPTION_CONTINUE_SEARCH;

  // To make sure an unrelated exception is not swallowed by the exception
  // handler, the address where the exception happened is verified.
  const EXCEPTION_RECORD* exception_record =
      exception_information->ExceptionRecord;
  const DWORD access_violation_address =
      exception_record->ExceptionInformation[1];

  uintptr_t range_start = reinterpret_cast<uintptr_t>(module_load_address);
  uintptr_t range_end = range_start + size_of_image;
  if (range_start > access_violation_address ||
      range_end <= access_violation_address) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

// Wrapper around GetModuleTimeDateStamp that handles a potential
// EXCEPTION_ACCESS_VIOLATION that can happen if the |module_load_address| is
// accessed after the module is unloaded. Also ensures that the expected module
// is loaded at this address.
bool TryGetModuleTimeDateStamp(void* module_load_address,
                               const base::FilePath& module_path,
                               uint32_t size_of_image,
                               uint32_t* time_date_stamp) {
  __try {
    // Make sure it's the correct module, to protect against a potential race
    // where a new module was loaded at the same address. This is safe because
    // the only possibles races are either there was a module loaded at
    // |module_load_address| and it was unloaded, or there was no module loaded
    // at |module_load_address| and a new one took its place.
    wchar_t module_file_name[MAX_PATH];
    DWORD size =
        ::GetModuleFileName(reinterpret_cast<HMODULE>(module_load_address),
                            module_file_name, ARRAYSIZE(module_file_name));
    if (!size || !base::FilePath::CompareEqualIgnoreCase(module_path.value(),
                                                         module_file_name)) {
      return false;
    }
    if (size_of_image != GetModuleSizeOfImage(module_load_address))
      return false;

    *time_date_stamp = GetModuleTimeDateStamp(module_load_address);
  } __except(FilterAccessViolation(GetExceptionCode(),
                                    GetExceptionInformation(),
                                    module_load_address, size_of_image)) {
    return false;
  }
  return true;
}

void ShowCloseBrowserFirstMessageBox() {
  chrome::ShowWarningMessageBox(
      nullptr, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(IDS_UNINSTALL_CLOSE_APP));
}

// Updates all Progressive Web App launchers in |profile_dir| to the latest
// version.
void UpdatePwaLaunchersForProfile(const base::FilePath& profile_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_dir);
  if (!profile) {
    // The profile was unloaded.
    return;
  }
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider)
    return;
  web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();

  // Create a vector of all PWA-launcher paths in |profile_dir|.
  std::vector<base::FilePath> pwa_launcher_paths;
  for (const webapps::AppId& app_id : registrar.GetAppIds()) {
    base::FilePath web_app_path =
        web_app::GetOsIntegrationResourcesDirectoryForApp(profile_dir, app_id,
                                                          GURL());
    web_app_path = web_app_path.Append(web_app::GetAppSpecificLauncherFilename(
        base::UTF8ToWide(registrar.GetAppShortName(app_id))));
    pwa_launcher_paths.push_back(std::move(web_app_path));
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&web_app::UpdatePwaLaunchers,
                     std::move(pwa_launcher_paths)));
}

void MigratePinnedTaskBarShortcutsIfNeeded() {
  // Update this number when users should go through a taskbar shortcut
  // migration again. The last reason to do this was crrev.com/798174. @
  // 86.0.4231.0.
  //
  // Note: If shortcut updates need to be done once after a future OS upgrade,
  // that should be done by re-versioning Active Setup (see //chrome/installer
  // and https://crbug.com/577697 for details).
  const base::Version kLastVersionNeedingMigration({86, 0, 4231, 0});

  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    const base::Version last_version_migrated(
        local_state->GetString(prefs::kShortcutMigrationVersion));
    if (!last_version_migrated.IsValid() ||
        last_version_migrated < kLastVersionNeedingMigration) {
      shell_integration::win::MigrateTaskbarPins(base::BindOnce(
          &PrefService::SetString, base::Unretained(local_state),
          prefs::kShortcutMigrationVersion, version_info::GetVersionNumber()));
    }
  }
}

void MaybeBlockDynamicCodeForBrowserProcess() {
  // This mitigation is not enabled if running in single-process mode or the GPU
  // is in-process. It is also not enabled if the sandbox is disabled.
  const char* const kUnsupportedSwitches[] = {
      switches::kSingleProcess, switches::kInProcessGPU,
      sandbox::policy::switches::kNoSandbox};

  for (const auto* unsupported_switch : kUnsupportedSwitches) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(unsupported_switch)) {
      return;
    }
  }

  bool block_dynamic_code =
      base::FeatureList::IsEnabled(features::kBrowserDynamicCodeDisabled);

  PrefService* local_state = g_browser_process->local_state();
  // Policy intentionally takes precedence over the feature.
  if (local_state->IsManagedPreference(prefs::kDynamicCodeSettings) &&
      local_state->GetInteger(prefs::kDynamicCodeSettings) ==
          /*EnabledForBrowser=*/1) {
    block_dynamic_code = true;
  }

  if (block_dynamic_code) {
    // This must happen when Chrome is still in single-threaded mode, in
    // particular, before the launcher thread has been created.
    sandbox::SandboxFactory::GetBrokerServices()
        ->RatchetDownSecurityMitigations(
            sandbox::MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT);
  }
}
// This error message is not localized because we failed to load the
// localization data files.
const char kMissingLocaleDataTitle[] = "Missing File Error";

// TODO(http://crbug.com/338969): This should be used on Linux Aura as well.
const char kMissingLocaleDataMessage[] =
    "Unable to find locale data files. Please reinstall.";

}  // namespace

int DoUninstallTasks(bool chrome_still_running) {
  // We want to show a warning to user (and exit) if Chrome is already running
  // *before* we show the uninstall confirmation dialog box. But while the
  // uninstall confirmation dialog is up, user might start Chrome, so we
  // check once again after user acknowledges Uninstall dialog.
  if (chrome_still_running) {
    ShowCloseBrowserFirstMessageBox();
    return chrome::RESULT_CODE_UNINSTALL_CHROME_ALIVE;
  }
  int result = chrome::ShowUninstallBrowserPrompt();
  if (browser_util::IsBrowserAlreadyRunning()) {
    ShowCloseBrowserFirstMessageBox();
    return chrome::RESULT_CODE_UNINSTALL_CHROME_ALIVE;
  }

  if (result != chrome::RESULT_CODE_UNINSTALL_USER_CANCEL) {
    // The following actions are just best effort.
    VLOG(1) << "Executing uninstall actions";
    // Remove shortcuts targeting chrome.exe or chrome_proxy.exe.
    base::FilePath install_dir;
    if (base::PathService::Get(base::DIR_EXE, &install_dir)) {
      std::vector<base::FilePath> shortcut_targets{
          install_dir.Append(installer::kChromeExe),
          install_dir.Append(installer::kChromeProxyExe)};
      ShellUtil::RemoveAllShortcuts(ShellUtil::CURRENT_USER, shortcut_targets);
    }
  }

  return result;
}

// ChromeBrowserMainPartsWin ---------------------------------------------------

ChromeBrowserMainPartsWin::ChromeBrowserMainPartsWin(bool is_integration_test,
                                                     StartupData* startup_data)
    : ChromeBrowserMainParts(is_integration_test, startup_data) {}

ChromeBrowserMainPartsWin::~ChromeBrowserMainPartsWin() = default;

void ChromeBrowserMainPartsWin::ToolkitInitialized() {
  DCHECK_NE(base::PlatformThread::CurrentId(), base::kInvalidThreadId);
  gfx::CrashIdHelper::RegisterMainThread(base::PlatformThread::CurrentId());
  ChromeBrowserMainParts::ToolkitInitialized();
  gfx::win::SetAdjustFontCallback(&l10n_util::AdjustUiFont);
  gfx::win::SetGetMinimumFontSizeCallback(&GetMinimumFontSize);
}

void ChromeBrowserMainPartsWin::PreCreateMainMessageLoop() {
  // installer_util references strings that are normally compiled into
  // setup.exe.  In Chrome, these strings are in the locale files.
  SetupInstallerUtilStrings();

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // Initialize the OSCrypt.
  bool os_crypt_init = OSCrypt::Init(local_state);
  DCHECK(os_crypt_init);

  base::SetExtraNoExecuteAllowedPath(chrome::DIR_USER_DATA);

  ChromeBrowserMainParts::PreCreateMainMessageLoop();
  if (!is_integration_test()) {
    // Make sure that we know how to handle exceptions from the message loop.
    InitializeWindowProcExceptions();
  }
}

int ChromeBrowserMainPartsWin::PreCreateThreads() {
  static constexpr std::string_view kIsEnterpriseManaged =
      "is-enterprise-managed";
  crash_keys::AllocateCrashKeyInBrowserAndChildren(
      kIsEnterpriseManaged,
      policy::ManagementServiceFactory::GetForPlatform()
                  ->GetManagementAuthorityTrustworthiness() >=
              policy::ManagementAuthorityTrustworthiness::TRUSTED
          ? "yes"
          : "no");

  // Set crash keys containing the registry values used to determine Chrome's
  // update channel at process startup; see https://crbug.com/579504.
  const auto& details = install_static::InstallDetails::Get();

  static crash_reporter::CrashKeyString<50> ap_value("ap");
  ap_value.Set(base::WideToUTF8(details.update_ap()));

  static crash_reporter::CrashKeyString<32> update_cohort_name("cohort-name");
  update_cohort_name.Set(base::WideToUTF8(details.update_cohort_name()));

  if (chrome::GetChannel() == version_info::Channel::CANARY) {
    content::RenderProcessHost::SetHungRendererAnalysisFunction(
        &DumpHungRendererProcessImpl);
  }

  // Pass the value of the UiAutomationProviderEnabled enterprise policy, if
  // set, down to the accessibility platform after the platform is initialized
  // in BrowserMainLoop::PostCreateMainMessageLoop() but before any UI is
  // created.
  if (auto* local_state = g_browser_process->local_state(); local_state) {
    if (auto* pref =
            local_state->FindPreference(prefs::kUiAutomationProviderEnabled);
        pref && pref->IsManaged()) {
      ui::AXPlatform::GetInstance().SetUiaProviderEnabled(
          pref->GetValue()->GetBool());
    }
  }

  return ChromeBrowserMainParts::PreCreateThreads();
}

void ChromeBrowserMainPartsWin::PostMainMessageLoopRun() {
  base::ImportantFileWriterCleaner::GetInstance().Stop();

  // The `ProfileManager` has been destroyed, so no new platform authentication
  // requests will be created.
  platform_auth_policy_observer_.reset();

  ChromeBrowserMainParts::PostMainMessageLoopRun();
}

void ChromeBrowserMainPartsWin::PostEarlyInitialization() {
  MaybeBlockDynamicCodeForBrowserProcess();

  ChromeBrowserMainParts::PostEarlyInitialization();
}

void ChromeBrowserMainPartsWin::ShowMissingLocaleMessageBox() {
  ui::MessageBox(NULL, base::ASCIIToWide(kMissingLocaleDataMessage),
                 base::ASCIIToWide(kMissingLocaleDataTitle),
                 MB_OK | MB_ICONERROR | MB_TOPMOST);
}

void ChromeBrowserMainPartsWin::PreProfileInit() {
  ChromeBrowserMainParts::PreProfileInit();

  // Create the module database and hook up the in-process module watcher. This
  // needs to be done before any child processes are initialized as the
  // `ModuleDatabase` is an endpoint for IPC from child processes.
  SetupModuleDatabase(&module_watcher_);

  // Start up the platform auth SSO policy observer.
  PrefService* const local_state = g_browser_process->local_state();
  if (local_state)
    platform_auth_policy_observer_ =
        std::make_unique<PlatformAuthPolicyObserver>(local_state);

  if (base::FeatureList::IsEnabled(features::kWinSystemLocationPermission) &&
      !device::GeolocationSystemPermissionManager::GetInstance()) {
    device::GeolocationSystemPermissionManager::SetInstance(
        device::SystemGeolocationSourceWin::
            CreateGeolocationSystemPermissionManager());
  }
}

void ChromeBrowserMainPartsWin::PostProfileInit(Profile* profile,
                                                bool is_initial_profile) {
  ChromeBrowserMainParts::PostProfileInit(profile, is_initial_profile);

  // The setup below is intended to run for only the initial profile.
  if (!is_initial_profile)
    return;

  // If Chrome was launched by a Progressive Web App launcher that needs to be
  // updated, update all launchers for this profile.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppId) &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPwaLauncherVersion) != chrome::kChromeVersion) {
    content::BrowserThread::PostBestEffortTask(
        FROM_HERE, base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindOnce(&UpdatePwaLaunchersForProfile, profile->GetPath()));
  }
}

void ChromeBrowserMainPartsWin::PostBrowserStart() {
  ChromeBrowserMainParts::PostBrowserStart();

  // Verify that the delay load helper hooks are in place. This cannot be tested
  // from unit tests, so rely on this failing here.
  DCHECK(__pfnDliFailureHook2);

  InitializeChromeElf();

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  if constexpr (kShouldRecordActiveUse) {
    did_run_updater_.emplace();
  }
#endif

  // Record Processor Metrics. This is very low priority, hence posting as
  // BEST_EFFORT to start after Chrome startup has completed.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DelayedRecordProcessorMetrics));

  // Write current executable path to the User Data directory to inform
  // Progressive Web App launchers, which run from within the User Data
  // directory, which chrome.exe to launch from.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&web_app::WriteChromePathToLastBrowserFile,
                     user_data_dir()));

  // Record the result of the latest Progressive Web App launcher launch.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&web_app::RecordPwaLauncherResult));

  // Possibly migrate pinned taskbar shortcuts.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&MigratePinnedTaskBarShortcutsIfNeeded));

  // Send an accessibility announcement if this launch originated from the
  // installer.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFromInstaller)) {
    AnnounceInActiveBrowser(l10n_util::GetStringUTF16(IDS_WELCOME_TO_CHROME));
  }

  // Some users are getting stuck in compatibility mode. Try to help them
  // escape; see http://crbug.com/581499.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce([]() {
        base::FilePath current_exe;
        if (base::PathService::Get(base::FILE_EXE, &current_exe)) {
          RemoveAppCompatEntries(current_exe);
        }
      }));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // os_update_handler is a separate process, so mirror the status of the
  // feature to the local state on its behalf.
  g_browser_process->local_state()->SetBoolean(
      prefs::kOsUpdateHandlerEnabled,
      base::FeatureList::IsEnabled(features::kRegisterOsUpdateHandlerWin));
#endif  // GOOGLE_CHROME_BRANDING

  base::ImportantFileWriterCleaner::GetInstance().Start();
}

// static
void ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment(
    const base::CommandLine& parsed_command_line) {
  // Clear this var so child processes don't show the dialog by default.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->UnSetVar(env_vars::kShowRestart);

  // For non-interactive tests we don't restart on crash.
  if (env->HasVar(env_vars::kHeadless))
    return;

  // If the known command-line test options are used we don't create the
  // environment block which means we don't get the restart dialog.
  if (parsed_command_line.HasSwitch(switches::kBrowserCrashTest) ||
      parsed_command_line.HasSwitch(switches::kNoErrorDialogs))
    return;

  // The encoding we use for the info is "title|context|direction" where
  // direction is either env_vars::kRtlLocale or env_vars::kLtrLocale depending
  // on the current locale.
  std::u16string dlg_strings(
      l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_TITLE));
  dlg_strings.push_back('|');
  std::u16string adjusted_string(
      l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_CONTENT));
  base::i18n::AdjustStringForLocaleDirection(&adjusted_string);
  dlg_strings.append(adjusted_string);
  dlg_strings.push_back('|');
  dlg_strings.append(base::ASCIIToUTF16(
      base::i18n::IsRTL() ? env_vars::kRtlLocale : env_vars::kLtrLocale));

  env->SetVar(env_vars::kRestartInfo, base::UTF16ToUTF8(dlg_strings));
}

// static
void ChromeBrowserMainPartsWin::RegisterApplicationRestart(
    const base::CommandLine& parsed_command_line) {
  base::ScopedNativeLibrary library(base::FilePath(L"kernel32.dll"));
  // Get the function pointer for RegisterApplicationRestart.
  RegisterApplicationRestartProc register_application_restart =
      reinterpret_cast<RegisterApplicationRestartProc>(
          library.GetFunctionPointer("RegisterApplicationRestart"));
  if (!register_application_restart) {
    LOG(WARNING) << "Cannot find RegisterApplicationRestart in kernel32.dll";
    return;
  }
  // Restart Chrome if the computer is restarted as the result of an update.
  // This could be extended to handle crashes, hangs, and patches.
  const auto command_line_string =
      GetRestartCommandLine(parsed_command_line).GetCommandLineString();
  HRESULT hr = register_application_restart(
      command_line_string.c_str(),
      RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_PATCH);
  if (FAILED(hr)) {
    if (hr == E_INVALIDARG) {
      LOG(WARNING) << "Command line too long for RegisterApplicationRestart: "
                   << command_line_string;
    } else {
      LOG(WARNING) << "RegisterApplicationRestart failed. hr: " << hr
                   << ", command_line: " << command_line_string;
    }
  }
}

// static
int ChromeBrowserMainPartsWin::HandleIconsCommands(
    const base::CommandLine& parsed_command_line) {
  if (parsed_command_line.HasSwitch(switches::kHideIcons)) {
    // TODO(crbug.com/41329700): This is not up-to-date and not localized.
    // Figure out if the --hide-icons and --show-icons switches are still used.
    std::u16string cp_applet = u"Programs and Features";
    const std::u16string msg =
        l10n_util::GetStringFUTF16(IDS_HIDE_ICONS_NOT_SUPPORTED, cp_applet);
    const std::u16string caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    const UINT flags = MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST;
    if (IDOK == ui::MessageBox(NULL, base::AsWString(msg),
                               base::AsWString(caption), flags)) {
      ShellExecute(NULL, NULL, L"appwiz.cpl", NULL, NULL, SW_SHOWNORMAL);
    }

    // Exit as we are not launching the browser.
    return content::RESULT_CODE_NORMAL_EXIT;
  }
  // We don't hide icons so we shouldn't do anything special to show them
  return chrome::RESULT_CODE_UNSUPPORTED_PARAM;
}

// static
bool ChromeBrowserMainPartsWin::CheckMachineLevelInstall() {
  base::Version version =
      InstallUtil::GetChromeVersion(true /* system_install */);
  if (version.IsValid()) {
    base::FilePath exe_path;
    base::PathService::Get(base::DIR_EXE, &exe_path);
    const base::FilePath user_exe_path(
        installer::GetInstalledDirectory(/*system_install=*/false));
    if (base::FilePath::CompareEqualIgnoreCase(exe_path.value(),
                                               user_exe_path.value())) {
      base::CommandLine uninstall_cmd(
          InstallUtil::GetChromeUninstallCmd(false));
      if (!uninstall_cmd.GetProgram().empty()) {
        uninstall_cmd.AppendSwitch(installer::switches::kSelfDestruct);
        uninstall_cmd.AppendSwitch(installer::switches::kForceUninstall);
        uninstall_cmd.AppendSwitch(
            installer::switches::kDoNotRemoveSharedItems);

        // Trigger Active Setup for the system-level Chrome to make sure
        // per-user shortcuts to the system-level Chrome are created. Skip this
        // if the system-level Chrome will undergo first run anyway, as Active
        // Setup is triggered on system-level Chrome's first run.
        // TODO(gab): Instead of having callers of Active Setup think about
        // other callers, have Active Setup itself register when it ran and
        // no-op otherwise (http://crbug.com/346843).
        if (!first_run::IsChromeFirstRun())
          uninstall_cmd.AppendSwitch(installer::switches::kTriggerActiveSetup);

        const base::FilePath setup_exe(uninstall_cmd.GetProgram());
        const std::wstring params(uninstall_cmd.GetArgumentsString());

        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOASYNC;
        sei.nShow = SW_SHOWNORMAL;
        sei.lpFile = setup_exe.value().c_str();
        sei.lpParameters = params.c_str();

        if (!::ShellExecuteEx(&sei))
          DPCHECK(false);
      }
      return true;
    }
  }
  return false;
}

std::wstring TranslationDelegate::GetLocalizedString(int installer_string_id) {
  int resource_id = 0;
  switch (installer_string_id) {
    // HANDLE_STRING is used by the DO_STRING_MAPPING macro which is in the
    // generated header installer_util_strings.h.
#define HANDLE_STRING(base_id, chrome_id) \
  case base_id: \
    resource_id = chrome_id; \
    break;
    DO_STRING_MAPPING
#undef HANDLE_STRING
  default:
    NOTREACHED_IN_MIGRATION();
  }
  if (resource_id)
    return base::UTF16ToWide(l10n_util::GetStringUTF16(resource_id));
  return std::wstring();
}

// static
void ChromeBrowserMainPartsWin::SetupInstallerUtilStrings() {
  static base::NoDestructor<TranslationDelegate> delegate;
  installer::SetTranslationDelegate(delegate.get());
}

// static
base::CommandLine ChromeBrowserMainPartsWin::GetRestartCommandLine(
    const base::CommandLine& command_line) {
  base::CommandLine restart_command(base::CommandLine::NO_PROGRAM);
  base::CommandLine::SwitchMap switches = command_line.GetSwitches();

  // Remove flag switches added by about::flags.
  about_flags::RemoveFlagsSwitches(&switches);

  // Remove switches that should never be conveyed to the restart.
  switches.erase(switches::kFromInstaller);

  // Add remaining switches, but not non-switch arguments.
  for (const auto& it : switches)
    restart_command.AppendSwitchNative(it.first, it.second);

  if (!command_line.HasSwitch(switches::kRestoreLastSession))
    restart_command.AppendSwitch(switches::kRestoreLastSession);

  // This is used when recording launch mode metric.
  if (!command_line.HasSwitch(switches::kRestart))
    restart_command.AppendSwitch(switches::kRestart);

  // TODO(crbug.com/41459588): Remove other unneeded switches, including
  // duplicates, perhaps harmonize with switches::RemoveSwitchesForAutostart.
  return restart_command;
}

// Used as the callback for ModuleWatcher events in this process. Dispatches
// them to the ModuleDatabase.
// Note: This callback may be invoked on any thread, even those not owned by the
//       task scheduler, under the loader lock, directly on the thread where the
//       DLL is currently loading.
void ChromeBrowserMainPartsWin::OnModuleEvent(
    const ModuleWatcher::ModuleEvent& event) {
  {
    TRACE_EVENT1("browser", "OnModuleEvent", "module_path",
                 event.module_path.BaseName().AsUTF8Unsafe());

    switch (event.event_type) {
      case ModuleWatcher::ModuleEventType::kModuleAlreadyLoaded: {
        // kModuleAlreadyLoaded comes from the enumeration of loaded modules
        // using CreateToolhelp32Snapshot().
        uint32_t time_date_stamp = 0;
        if (TryGetModuleTimeDateStamp(event.module_load_address,
                                      event.module_path, event.module_size,
                                      &time_date_stamp)) {
          ModuleDatabase::HandleModuleLoadEvent(
              content::PROCESS_TYPE_BROWSER, event.module_path,
              event.module_size, time_date_stamp);
        } else {
          // Failed to get the TimeDateStamp directly from memory. The next step
          // to try is to read the file on disk. This must be done in a blocking
          // task.
          base::ThreadPool::PostTask(
              FROM_HERE,
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
              base::BindOnce(&HandleModuleLoadEventWithoutTimeDateStamp,
                             event.module_path, event.module_size));
        }
        break;
      }
      case ModuleWatcher::ModuleEventType::kModuleLoaded: {
        ModuleDatabase::HandleModuleLoadEvent(
            content::PROCESS_TYPE_BROWSER, event.module_path, event.module_size,
            GetModuleTimeDateStamp(event.module_load_address));
        break;
      }
    }
  }
}

// Helper function for initializing the module database subsystem and populating
// the provided |module_watcher|.
void ChromeBrowserMainPartsWin::SetupModuleDatabase(
    std::unique_ptr<ModuleWatcher>* module_watcher) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(module_watcher);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly disable the third-party modules blocking.
  //
  // Because the blocking code lives in chrome_elf, it is not possible to check
  // the feature (via the FeatureList API) or the policy to control whether it
  // is enabled or not.
  //
  // What truly controls if the blocking is enabled is the presence of the
  // module blocklist cache file. This means that to disable the feature, the
  // cache must be deleted and the browser relaunched.
  if (!ModuleDatabase::IsThirdPartyBlockingPolicyEnabled() ||
      !ModuleBlocklistCacheUpdater::IsBlockingEnabled())
    ThirdPartyConflictsManager::DisableThirdPartyModuleBlocking(
        base::ThreadPool::CreateTaskRunner(
            {base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
             base::MayBlock()})
            .get());
#endif

  bool third_party_blocking_policy_enabled =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ModuleDatabase::IsThirdPartyBlockingPolicyEnabled();
#else
      false;
#endif

  ModuleDatabase::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&InitializeModuleDatabase,
                                third_party_blocking_policy_enabled));

  *module_watcher = ModuleWatcher::Create(base::BindRepeating(
      &ChromeBrowserMainPartsWin::OnModuleEvent, base::Unretained(this)));
}
