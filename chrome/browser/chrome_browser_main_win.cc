// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_win.h"

#include <shellapi.h>
#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include <algorithm>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/enterprise_util.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "base/win/pe_image.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/wrapped_window_proc.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_util_win.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/browser/win/browser_util.h"
#include "chrome/browser/win/chrome_elf_init.h"
#include "chrome/browser/win/conflicts/enumerate_input_method_editors.h"
#include "chrome/browser/win/conflicts/enumerate_shell_extensions.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/module_event_sink_impl.h"
#include "chrome/chrome_watcher/chrome_watcher_main_api.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/shell_util.h"
#include "components/crash/content/app/crash_export_thunks.h"
#include "components/crash/content/app/dump_hung_process_with_ptype.h"
#include "components/crash/core/common/crash_key.h"
#include "components/os_crypt/os_crypt.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/win/hidden_window.h"
#include "ui/base/win/message_box_win.h"
#include "ui/display/win/dpi.h"
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

// gfx::Font callbacks
void AdjustUIFont(gfx::win::FontAdjustment* font_adjustment) {
  l10n_util::NeedOverrideDefaultUIFont(&font_adjustment->font_family_override,
                                       &font_adjustment->font_scale);
  font_adjustment->font_scale *= display::win::GetAccessibilityFontScale();
}

int GetMinimumFontSize() {
  int min_font_size;
  base::StringToInt(l10n_util::GetStringUTF16(IDS_MINIMUM_UI_FONT_SIZE),
                    &min_font_size);
  return min_font_size;
}

class TranslationDelegate : public installer::TranslationDelegate {
 public:
  base::string16 GetLocalizedString(int installer_string_id) override;
};

void DetectFaultTolerantHeap() {
  enum FTHFlags {
    FTH_HKLM = 1,
    FTH_HKCU = 2,
    FTH_ACLAYERS_LOADED = 4,
    FTH_ACXTRNAL_LOADED = 8,
    FTH_FLAGS_COUNT = 16
  };

  // The Fault Tolerant Heap (FTH) is enabled on some customer machines and is
  // affecting their performance. We need to know how many machines are
  // affected in order to decide what to do.

  // The main way that the FTH is enabled is by having a value set in
  // HKLM\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers
  // whose name is the full path to the executable and whose data is the
  // string FaultTolerantHeap. Some documents suggest that this data may also
  // be found in HKCU. There have also been cases observed where this registry
  // key is set but the FTH is not enabled, so we also look for AcXtrnal.dll
  // and AcLayers.dll which are used to implement the FTH on Windows 7 and 8
  // respectively.

  // Get the module path so that we can look for it in the registry.
  wchar_t module_path[MAX_PATH];
  GetModuleFileName(NULL, module_path, ARRAYSIZE(module_path));
  // Force null-termination, necessary on Windows XP.
  module_path[ARRAYSIZE(module_path)-1] = 0;

  const wchar_t* const kRegPath = L"Software\\Microsoft\\Windows NT\\"
        L"CurrentVersion\\AppCompatFlags\\Layers";
  const wchar_t* const kFTHData = L"FaultTolerantHeap";
  // We always want to read from the 64-bit version of the registry if present,
  // since that is what the OS looks at, even for 32-bit processes.
  const DWORD kRegFlags = KEY_READ | KEY_WOW64_64KEY;

  base::win::RegKey FTH_HKLM_reg(HKEY_LOCAL_MACHINE, kRegPath, kRegFlags);
  FTHFlags detected = FTHFlags();
  base::string16 chrome_app_compat;
  if (FTH_HKLM_reg.ReadValue(module_path, &chrome_app_compat) == 0) {
    // This *usually* indicates that the fault tolerant heap is enabled.
    if (wcsicmp(chrome_app_compat.c_str(), kFTHData) == 0)
      detected = static_cast<FTHFlags>(detected | FTH_HKLM);
  }

  base::win::RegKey FTH_HKCU_reg(HKEY_CURRENT_USER, kRegPath, kRegFlags);
  if (FTH_HKCU_reg.ReadValue(module_path, &chrome_app_compat) == 0) {
    if (wcsicmp(chrome_app_compat.c_str(), kFTHData) == 0)
      detected = static_cast<FTHFlags>(detected | FTH_HKCU);
  }

  // Look for the DLLs used to implement the FTH and other compat hacks.
  if (GetModuleHandleW(L"AcLayers.dll") != NULL)
    detected = static_cast<FTHFlags>(detected | FTH_ACLAYERS_LOADED);
  if (GetModuleHandleW(L"AcXtrnal.dll") != NULL)
    detected = static_cast<FTHFlags>(detected | FTH_ACXTRNAL_LOADED);

  UMA_HISTOGRAM_ENUMERATION("FaultTolerantHeap", detected, FTH_FLAGS_COUNT);
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
  UMA_HISTOGRAM_BOOLEAN("ThirdPartyModules.TimeDateStampObtained",
                        got_time_date_stamp);

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

// Used as the callback for ModuleWatcher events in this process. Dispatches
// them to the ModuleDatabase.
// Note: This callback may be invoked on any thread, even those not owned by the
//       task scheduler, under the loader lock, directly on the thread where the
//       DLL is currently loading.
void OnModuleEvent(const ModuleWatcher::ModuleEvent& event) {
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
            content::PROCESS_TYPE_BROWSER, event.module_path, event.module_size,
            time_date_stamp);
      } else {
        // Failed to get the TimeDateStamp directly from memory. The next step
        // to try is to read the file on disk. This must be done in a blocking
        // task.
        base::PostTask(
            FROM_HERE,
            {base::ThreadPool(), base::MayBlock(),
             base::TaskPriority::BEST_EFFORT,
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

// Helper function for initializing the module database subsystem and populating
// the provided |module_watcher|.
void SetupModuleDatabase(std::unique_ptr<ModuleWatcher>* module_watcher) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(module_watcher);

  bool third_party_blocking_policy_enabled =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ModuleDatabase::IsThirdPartyBlockingPolicyEnabled();
#else
      false;
#endif

  ModuleDatabase::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&InitializeModuleDatabase,
                                third_party_blocking_policy_enabled));

  *module_watcher = ModuleWatcher::Create(base::BindRepeating(&OnModuleEvent));
}

void ShowCloseBrowserFirstMessageBox() {
  chrome::ShowWarningMessageBox(
      nullptr, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(IDS_UNINSTALL_CLOSE_APP));
}

void MaybePostSettingsResetPrompt() {
  if (base::FeatureList::IsEnabled(safe_browsing::kSettingsResetPrompt)) {
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
        base::Bind(safe_browsing::MaybeShowSettingsResetPromptWithDelay));
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
    // TODO(gab): Look into removing this code which is now redundant with the
    // work done by setup.exe on uninstall.
    VLOG(1) << "Executing uninstall actions";
    base::FilePath chrome_exe;
    if (base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
      ShellUtil::ShortcutLocation user_shortcut_locations[] = {
          ShellUtil::SHORTCUT_LOCATION_DESKTOP,
          ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
          ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT,
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
      };
      for (size_t i = 0; i < base::size(user_shortcut_locations); ++i) {
        if (!ShellUtil::RemoveShortcuts(user_shortcut_locations[i],
                                        ShellUtil::CURRENT_USER, chrome_exe)) {
          VLOG(1) << "Failed to delete shortcut at location "
                  << user_shortcut_locations[i];
        }
      }
    } else {
      NOTREACHED();
    }
  }
  return result;
}

// ChromeBrowserMainPartsWin ---------------------------------------------------

ChromeBrowserMainPartsWin::ChromeBrowserMainPartsWin(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainParts(parameters, startup_data) {}

ChromeBrowserMainPartsWin::~ChromeBrowserMainPartsWin() = default;

void ChromeBrowserMainPartsWin::ToolkitInitialized() {
  DCHECK_NE(base::PlatformThread::CurrentId(), base::kInvalidThreadId);
  gfx::CrashIdHelper::RegisterMainThread(base::PlatformThread::CurrentId());
  ChromeBrowserMainParts::ToolkitInitialized();
  gfx::win::SetAdjustFontCallback(&AdjustUIFont);
  gfx::win::SetGetMinimumFontSizeCallback(&GetMinimumFontSize);
  ui::CursorLoaderWin::SetCursorResourceModule(chrome::kBrowserResourcesDll);
}

void ChromeBrowserMainPartsWin::PreMainMessageLoopStart() {
  // installer_util references strings that are normally compiled into
  // setup.exe.  In Chrome, these strings are in the locale files.
  SetupInstallerUtilStrings();

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // Initialize the OSCrypt.
  bool os_crypt_init = OSCrypt::Init(local_state);
  DCHECK(os_crypt_init);

  ChromeBrowserMainParts::PreMainMessageLoopStart();
  if (!parameters().ui_task) {
    // Make sure that we know how to handle exceptions from the message loop.
    InitializeWindowProcExceptions();
  }
}

int ChromeBrowserMainPartsWin::PreCreateThreads() {
  // Record whether the machine is enterprise managed in a crash key. This will
  // be used to better identify whether crashes are from enterprise users.
  static crash_reporter::CrashKeyString<4> is_enterprise_managed(
      "is-enterprise-managed");
  is_enterprise_managed.Set(base::IsMachineExternallyManaged() ? "yes" : "no");

  // Set crash keys containing the registry values used to determine Chrome's
  // update channel at process startup; see https://crbug.com/579504.
  const auto& details = install_static::InstallDetails::Get();

  static crash_reporter::CrashKeyString<50> ap_value("ap");
  ap_value.Set(base::UTF16ToUTF8(details.update_ap()));

  static crash_reporter::CrashKeyString<32> update_cohort_name("cohort-name");
  update_cohort_name.Set(base::UTF16ToUTF8(details.update_cohort_name()));

  if (chrome::GetChannel() == version_info::Channel::CANARY) {
    content::RenderProcessHost::SetHungRendererAnalysisFunction(
        &DumpHungRendererProcessImpl);
  }

  return ChromeBrowserMainParts::PreCreateThreads();
}

void ChromeBrowserMainPartsWin::ShowMissingLocaleMessageBox() {
  ui::MessageBox(NULL, base::ASCIIToUTF16(kMissingLocaleDataMessage),
                 base::ASCIIToUTF16(kMissingLocaleDataTitle),
                 MB_OK | MB_ICONERROR | MB_TOPMOST);
}

void ChromeBrowserMainPartsWin::PostProfileInit() {
  ChromeBrowserMainParts::PostProfileInit();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly disable the third-party modules blocking.
  //
  // Because the blocking code lives in chrome_elf, it is not possible to check
  // the feature (via the FeatureList API) or the policy to control whether it
  // is enabled or not.
  //
  // What truly controls if the blocking is enabled is the presence of the
  // module blacklist cache file. This means that to disable the feature, the
  // cache must be deleted and the browser relaunched.
  if (!ModuleDatabase::IsThirdPartyBlockingPolicyEnabled() ||
      !ModuleBlacklistCacheUpdater::IsBlockingEnabled())
    ThirdPartyConflictsManager::DisableThirdPartyModuleBlocking(
        base::CreateTaskRunner(
            {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
             base::MayBlock()})
            .get());
#endif

  // Create the module database and hook up the in-process module watcher. This
  // needs to be done before any child processes are initialized as the
  // ModuleDatabase is an endpoint for IPC from child processes.
  SetupModuleDatabase(&module_watcher_);
}

void ChromeBrowserMainPartsWin::PostBrowserStart() {
  ChromeBrowserMainParts::PostBrowserStart();

  UMA_HISTOGRAM_BOOLEAN("Windows.Tablet",
      base::win::IsTabletDevice(nullptr, ui::GetHiddenWindow()));

  InitializeChromeElf();

  // Reset settings for the current profile if it's tagged to be reset after a
  // complete run of the Chrome Cleanup tool. If post-cleanup settings reset is
  // enabled, we delay checks for settings reset prompt until the scheduled
  // reset is finished.
  if (safe_browsing::PostCleanupSettingsResetter::IsEnabled()) {
    // Using last opened profiles, because we want to find reset the profile
    // that was open in the last Chrome run, which may not be open yet in
    // the current run.
    safe_browsing::PostCleanupSettingsResetter().ResetTaggedProfiles(
        g_browser_process->profile_manager()->GetLastOpenedProfiles(),
        base::BindOnce(&MaybePostSettingsResetPrompt),
        std::make_unique<
            safe_browsing::PostCleanupSettingsResetter::Delegate>());
  } else {
    MaybePostSettingsResetPrompt();
  }

  // Record UMA data about whether the fault-tolerant heap is enabled.
  // Use a delayed task to minimize the impact on startup time.
  base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                        base::BindOnce(&DetectFaultTolerantHeap),
                        base::TimeDelta::FromMinutes(1));
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
  base::string16 dlg_strings(
      l10n_util::GetStringUTF16(IDS_CRASH_RECOVERY_TITLE));
  dlg_strings.push_back('|');
  base::string16 adjusted_string(
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
  // The Windows Restart Manager expects a string of command line flags only,
  // without the program.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendArguments(parsed_command_line, false);
  if (!command_line.HasSwitch(switches::kRestoreLastSession))
    command_line.AppendSwitch(switches::kRestoreLastSession);

  // Restart Chrome if the computer is restarted as the result of an update.
  // This could be extended to handle crashes, hangs, and patches.
  const auto& command_line_string = command_line.GetCommandLineString();
  HRESULT hr = register_application_restart(
      command_line_string.c_str(),
      RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_PATCH);
  if (FAILED(hr)) {
    if (hr == E_INVALIDARG) {
      LOG(WARNING) << "Command line too long for RegisterApplicationRestart: "
                   << command_line_string;
    } else {
      NOTREACHED() << "RegisterApplicationRestart failed. hr: " << hr
                   << ", command_line: " << command_line_string;
    }
  }
}

// static
int ChromeBrowserMainPartsWin::HandleIconsCommands(
    const base::CommandLine& parsed_command_line) {
  if (parsed_command_line.HasSwitch(switches::kHideIcons)) {
    // TODO(740976): This is not up-to-date and not localized. Figure out if
    // the --hide-icons and --show-icons switches are still used.
    base::string16 cp_applet(L"Programs and Features");
    const base::string16 msg =
        l10n_util::GetStringFUTF16(IDS_HIDE_ICONS_NOT_SUPPORTED, cp_applet);
    const base::string16 caption = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    const UINT flags = MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST;
    if (IDOK == ui::MessageBox(NULL, msg, caption, flags))
      ShellExecute(NULL, NULL, L"appwiz.cpl", NULL, NULL, SW_SHOWNORMAL);

    // Exit as we are not launching the browser.
    return service_manager::RESULT_CODE_NORMAL_EXIT;
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
    std::wstring exe = exe_path.value();
    base::FilePath user_exe_path(installer::GetChromeInstallPath(false));
    if (base::FilePath::CompareEqualIgnoreCase(exe, user_exe_path.value())) {
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
        const base::string16 params(uninstall_cmd.GetArgumentsString());

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

base::string16 TranslationDelegate::GetLocalizedString(
    int installer_string_id) {
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
    NOTREACHED();
  }
  if (resource_id)
    return l10n_util::GetStringUTF16(resource_id);
  return base::string16();
}

// static
void ChromeBrowserMainPartsWin::SetupInstallerUtilStrings() {
  static base::NoDestructor<TranslationDelegate> delegate;
  installer::SetTranslationDelegate(delegate.get());
}
