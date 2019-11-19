// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_delegate.h"

#include <stddef.h>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event_impl.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_resource_bundle_helper.h"
#include "chrome/browser/defaults.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/heap_profiler_controller.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "chrome/gpu/chrome_content_gpu_client.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/gwp_asan/buildflags/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/profiling.h"
#include "content/public/common/service_names.mojom.h"
#include "extensions/common/constants.h"
#include "net/url_request/url_request.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/embedder/switches.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include <malloc.h>

#include <algorithm>

#include "base/debug/close_handle_hook_win.h"
#include "base/win/atl.h"
#include "chrome/child/v8_crashpad_support_win.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/child_process_logging.h"
#include "sandbox/win/src/sandbox.h"
#include "ui/base/resource/resource_bundle_win.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "chrome/app/chrome_main_mac.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/mac/relauncher.h"
#include "chrome/browser/shell_integration.h"
#include "components/crash/core/common/objc_zombie.h"
#include "ui/base/l10n/l10n_util_mac.h"
#endif

#if defined(OS_POSIX)
#include <locale.h>
#include <signal.h>

#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/app/shutdown_signal_handlers_posix.h"
#endif

#if BUILDFLAG(ENABLE_NACL) && defined(OS_LINUX)
#include "components/nacl/common/nacl_paths.h"
#include "components/nacl/zygote/nacl_fork_delegate_linux.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/dbus/dbus_helper.h"
#include "chrome/browser/chromeos/startup_settings_cache.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/hugepage_text/hugepage_text.h"
#include "chromeos/memory/kstaled.h"
#include "chromeos/memory/memory.h"
#include "chromeos/memory/swap_configuration.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/java_exception_reporter.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/crash/pure_java_exception_handler.h"
#include "chrome/common/chrome_descriptors.h"
#include "net/android/network_change_notifier_factory_android.h"
#else  // defined(OS_ANDROID)
// Diagnostics is only available on non-android platforms.
#include "chrome/browser/diagnostics/diagnostics_controller.h"
#include "chrome/browser/diagnostics/diagnostics_writer.h"
#endif

#if defined(USE_X11)
#include <stdlib.h>
#include <string.h>
#include "ui/base/x/x11_util.h"  // nogncheck
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "components/crash/content/app/breakpad_linux.h"
#include "v8/include/v8-wasm-trap-handler-posix.h"
#include "v8/include/v8.h"
#endif

#if defined(OS_LINUX)
#include "base/environment.h"
#endif

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_ANDROID) || \
    defined(OS_LINUX)
#include "chrome/browser/policy/policy_path_parser.h"
#include "components/crash/content/app/crashpad.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/renderer/plugin/ppapi_entrypoints.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS) && (defined(CHROME_MULTIPLE_DLL_CHILD) || \
    !defined(CHROME_MULTIPLE_DLL_BROWSER))
#include "pdf/pdf_ppapi.h"
#endif

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/startup_data.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#endif

#if !defined(CHROME_MULTIPLE_DLL_BROWSER) && BUILDFLAG(ENABLE_PDF)
#include "chrome/child/pdf_child_init.h"
#endif

#if BUILDFLAG(ENABLE_GWP_ASAN)
#include "components/gwp_asan/client/gwp_asan.h"  // nogncheck
#endif

#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
base::LazyInstance<ChromeContentGpuClient>::DestructorAtExit
    g_chrome_content_gpu_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ChromeContentRendererClient>::DestructorAtExit
    g_chrome_content_renderer_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ChromeContentUtilityClient>::DestructorAtExit
    g_chrome_content_utility_client = LAZY_INSTANCE_INITIALIZER;
#endif

extern int NaClMain(const content::MainFunctionParams&);

#if !defined(OS_CHROMEOS)
extern int CloudPrintServiceProcessMain(const content::MainFunctionParams&);
#endif

const char* const ChromeMainDelegate::kNonWildcardDomainNonPortSchemes[] = {
    extensions::kExtensionScheme, chrome::kChromeSearchScheme};
const size_t ChromeMainDelegate::kNonWildcardDomainNonPortSchemesSize =
    base::size(kNonWildcardDomainNonPortSchemes);

namespace {

#if defined(OS_WIN)
// Early versions of Chrome incorrectly registered a chromehtml: URL handler,
// which gives us nothing but trouble. Avoid launching chrome this way since
// some apps fail to properly escape arguments.
bool HasDeprecatedArguments(const base::string16& command_line) {
  const wchar_t kChromeHtml[] = L"chromehtml:";
  base::string16 command_line_lower = base::ToLowerASCII(command_line);
  // We are only searching for ASCII characters so this is OK.
  return (command_line_lower.find(kChromeHtml) != base::string16::npos);
}

// If we try to access a path that is not currently available, we want the call
// to fail rather than show an error dialog.
void SuppressWindowsErrorDialogs() {
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode.
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
}

bool IsSandboxedProcess() {
  typedef bool (*IsSandboxedProcessFunc)();
  IsSandboxedProcessFunc is_sandboxed_process_func =
      reinterpret_cast<IsSandboxedProcessFunc>(
          GetProcAddress(GetModuleHandle(NULL), "IsSandboxedProcess"));
  return is_sandboxed_process_func && is_sandboxed_process_func();
}

bool UseHooks() {
#if defined(ARCH_CPU_64_BITS)
  return false;
#elif defined(NDEBUG)
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEV) {
    return true;
  }

  return false;
#else  // NDEBUG
  return true;
#endif
}

#endif  // defined(OS_WIN)

#if defined(OS_LINUX)
void AdjustLinuxOOMScore(const std::string& process_type) {
  // Browsers and zygotes should still be killable, but killed last.
  const int kZygoteScore = 0;
  // The minimum amount to bump a score by.  This is large enough that
  // even if it's translated into the old values, it will still go up
  // by at least one.
  const int kScoreBump = 100;
  // This is the lowest score that renderers and extensions start with
  // in the OomPriorityManager.
  const int kRendererScore = chrome::kLowestRendererOomScore;
  // For "miscellaneous" things, we want them after renderers,
  // but before plugins.
  const int kMiscScore = kRendererScore - kScoreBump;
  // We want plugins to die after the renderers.
  const int kPluginScore = kMiscScore - kScoreBump;
  int score = -1;

  DCHECK_GT(kMiscScore, 0);
  DCHECK_GT(kPluginScore, 0);

  if (process_type == switches::kPpapiPluginProcess) {
    score = kPluginScore;
  } else if (process_type == switches::kPpapiBrokerProcess) {
    // The broker should be killed before the PPAPI plugin.
    score = kPluginScore + kScoreBump;
  } else if (process_type == switches::kUtilityProcess ||
             process_type == switches::kGpuProcess ||
             process_type == switches::kCloudPrintServiceProcess ||
             process_type == service_manager::switches::kProcessTypeService) {
    score = kMiscScore;
#if BUILDFLAG(ENABLE_NACL)
  } else if (process_type == switches::kNaClLoaderProcess ||
             process_type == switches::kNaClLoaderNonSfiProcess) {
    score = kPluginScore;
#endif
  } else if (process_type == service_manager::switches::kZygoteProcess ||
             process_type ==
                 service_manager::switches::kProcessTypeServiceManager ||
             process_type.empty()) {
    // For zygotes and unlabeled process types, we want to still make
    // them killable by the OOM killer.
    score = kZygoteScore;
  } else if (process_type == switches::kRendererProcess) {
    LOG(WARNING) << "process type 'renderer' "
                 << "should be created through the zygote.";
    // When debugging, this process type can end up being run directly, but
    // this isn't the typical path for assigning the OOM score for it.  Still,
    // we want to assign a score that is somewhat representative for debugging.
    score = kRendererScore;
  } else {
    NOTREACHED() << "Unknown process type";
  }
  // In the case of a 0 score, still try to adjust it. Most likely the score is
  // 0 already, but it may not be if this process inherited a higher score from
  // its parent process.
  if (score > -1)
    base::AdjustOOMScore(base::GetCurrentProcId(), score);
}
#endif  // defined(OS_LINUX)

// Returns true if this subprocess type needs the ResourceBundle initialized
// and resources loaded.
bool SubprocessNeedsResourceBundle(const std::string& process_type) {
  return
#if defined(OS_LINUX)
      // The zygote process opens the resources for the renderers.
      process_type == service_manager::switches::kZygoteProcess ||
#endif
#if defined(OS_MACOSX)
      // Mac needs them too for scrollbar related images and for sandbox
      // profiles.
#if BUILDFLAG(ENABLE_NACL)
      process_type == switches::kNaClLoaderProcess ||
#endif
      process_type == switches::kPpapiPluginProcess ||
      process_type == switches::kPpapiBrokerProcess ||
      process_type == switches::kGpuProcess ||
#endif
      process_type == switches::kRendererProcess ||
      process_type == switches::kUtilityProcess;
}

#if defined(OS_POSIX)
// Check for --version and --product-version; return true if we encountered
// one of these switches and should exit now.
bool HandleVersionSwitches(const base::CommandLine& command_line) {
#if !defined(OS_MACOSX)
  if (command_line.HasSwitch(switches::kProductVersion)) {
    printf("%s\n", version_info::GetVersionNumber().c_str());
    return true;
  }
#endif

  if (command_line.HasSwitch(switches::kVersion)) {
    printf("%s %s %s\n", version_info::GetProductName().c_str(),
           version_info::GetVersionNumber().c_str(),
           chrome::GetChannelName().c_str());
    return true;
  }

  return false;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Show the man page if --help or -h is on the command line.
void HandleHelpSwitches(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kHelp) ||
      command_line.HasSwitch(switches::kHelpShort)) {
    base::FilePath binary(command_line.argv()[0]);
    execlp("man", "man", binary.BaseName().value().c_str(), NULL);
    PLOG(FATAL) << "execlp failed";
  }
}
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
void SIGTERMProfilingShutdown(int signal) {
  content::Profiling::Stop();
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  CHECK_EQ(sigaction(SIGTERM, &sigact, NULL), 0);
  raise(signal);
}

void SetUpProfilingShutdownHandler() {
  struct sigaction sigact;
  sigact.sa_handler = SIGTERMProfilingShutdown;
  sigact.sa_flags = SA_RESETHAND;
  sigemptyset(&sigact.sa_mask);
  CHECK_EQ(sigaction(SIGTERM, &sigact, NULL), 0);
}
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)

#endif  // OS_POSIX

struct MainFunction {
  const char* name;
  int (*function)(const content::MainFunctionParams&);
};

// Initializes the user data dir. Must be called before InitializeLocalState().
void InitializeUserDataDir(base::CommandLine* command_line) {
#if defined(OS_WIN)
  // Reach out to chrome_elf for the truth on the user data directory.
  // Note that in tests, this links to chrome_elf_test_stubs.
  wchar_t user_data_dir_buf[MAX_PATH], invalid_user_data_dir_buf[MAX_PATH];

  // In tests this may return false, implying the user data dir should be unset.
  if (GetUserDataDirectoryThunk(
          user_data_dir_buf, base::size(user_data_dir_buf),
          invalid_user_data_dir_buf, base::size(invalid_user_data_dir_buf))) {
    base::FilePath user_data_dir(user_data_dir_buf);
    if (invalid_user_data_dir_buf[0] != 0) {
      chrome::SetInvalidSpecifiedUserDataDir(
          base::FilePath(invalid_user_data_dir_buf));
      command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
    }
    CHECK(base::PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_USER_DATA, user_data_dir, false, true));
  }
#else  // OS_WIN
  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_LINUX)
  // On Linux, Chrome does not support running multiple copies under different
  // DISPLAYs, so the profile directory can be specified in the environment to
  // support the virtual desktop use-case.
  if (user_data_dir.empty()) {
    std::string user_data_dir_string;
    std::unique_ptr<base::Environment> environment(base::Environment::Create());
    if (environment->GetVar("CHROME_USER_DATA_DIR", &user_data_dir_string) &&
        base::IsStringUTF8(user_data_dir_string)) {
      user_data_dir = base::FilePath::FromUTF8Unsafe(user_data_dir_string);
    }
  }
#endif  // OS_LINUX
#if defined(OS_MACOSX)
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
#endif  // OS_MAC

  const bool specified_directory_was_invalid =
      !user_data_dir.empty() &&
      !base::PathService::OverrideAndCreateIfNeeded(chrome::DIR_USER_DATA,
                                                    user_data_dir, false, true);
  // Save inaccessible or invalid paths so the user may be prompted later.
  if (specified_directory_was_invalid)
    chrome::SetInvalidSpecifiedUserDataDir(user_data_dir);

  // Warn and fail early if the process fails to get a user data directory.
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    // If an invalid command-line or policy override was specified, the user
    // will be given an error with that value. Otherwise, use the directory
    // returned by PathService (or the fallback default directory) in the error.
    if (!specified_directory_was_invalid) {
      // base::PathService::Get() returns false and yields an empty path if it
      // fails to create DIR_USER_DATA. Retrieve the default value manually to
      // display a more meaningful error to the user in that case.
      if (user_data_dir.empty())
        chrome::GetDefaultUserDataDirectory(&user_data_dir);
      chrome::SetInvalidSpecifiedUserDataDir(user_data_dir);
    }

    // The browser process (which is identified by an empty |process_type|) will
    // handle the error later; other processes that need the dir crash here.
    CHECK(process_type.empty()) << "Unable to get the user data directory "
                                << "for process type: " << process_type;
  }

  // Append the fallback user data directory to the commandline. Otherwise,
  // child or service processes will attempt to use the invalid directory.
  if (specified_directory_was_invalid)
    command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
#endif  // OS_WIN
}

#if !defined(OS_ANDROID)
void InitLogging(const std::string& process_type) {
  logging::OldFileDeletionState file_state =
      logging::APPEND_TO_OLD_LOG_FILE;
  if (process_type.empty()) {
    file_state = logging::DELETE_OLD_LOG_FILE;
  }
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  logging::InitChromeLogging(command_line, file_state);
}
#endif

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
void RecordMainStartupMetrics(base::TimeTicks exe_entry_point_ticks) {
  if (!exe_entry_point_ticks.is_null())
    startup_metric_utils::RecordExeMainEntryPointTicks(exe_entry_point_ticks);
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  // Record the startup process creation time on supported platforms.
  startup_metric_utils::RecordStartupProcessCreationTime(
      base::Process::Current().CreationTime());
#endif

// On Android the main entry point time is the time when the Java code starts.
// This happens before the shared library containing this code is even loaded.
// The Java startup code has recorded that time, but the C++ code can't fetch it
// from the Java side until it has initialized the JNI. See
// ChromeMainDelegateAndroid.
#if !defined(OS_ANDROID)
  startup_metric_utils::RecordMainEntryPointTime(base::TimeTicks::Now());
#endif
}
#endif  // !defined(CHROME_MULTIPLE_DLL_CHILD)

}  // namespace

ChromeMainDelegate::ChromeMainDelegate()
    : ChromeMainDelegate(base::TimeTicks()) {}

ChromeMainDelegate::ChromeMainDelegate(base::TimeTicks exe_entry_point_ticks) {
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
  // Record startup metrics in the browser process. For component builds, there
  // is no way to know the type of process (process command line is not yet
  // initialized), so the function below will also be called in renderers.
  // This doesn't matter as it simply sets global variables.
  RecordMainStartupMetrics(exe_entry_point_ticks);
#endif  // !defined(CHROME_MULTIPLE_DLL_CHILD)
}

ChromeMainDelegate::~ChromeMainDelegate() {
}

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
void ChromeMainDelegate::PostEarlyInitialization(bool is_running_tests) {
  // Chrome disallows cookies by default. All code paths that want to use
  // cookies need to go through one of Chrome's URLRequestContexts which have
  // a ChromeNetworkDelegate attached that selectively allows cookies again.
  net::URLRequest::SetDefaultCookiePolicyToBlock();

#if defined(OS_CHROMEOS)
  // The feature list depends on BrowserPolicyConnectorChromeOS which depends
  // on DBus, so initialize it here. Some D-Bus clients may depend on feature
  // list, so initialize them separately later at the end of this function.
  chromeos::InitializeDBus();
#endif

  DCHECK(startup_data_);
  auto* chrome_feature_list_creator =
      startup_data_->chrome_feature_list_creator();
  chrome_feature_list_creator->CreateFeatureList();
  PostFieldTrialInitialization();

  // Initializes the resource bundle and determines the locale.
  std::string actual_locale =
      LoadLocalState(chrome_feature_list_creator, is_running_tests);
  chrome_feature_list_creator->SetApplicationLocale(actual_locale);
  chrome_feature_list_creator->OverrideCachedUIStrings();

#if defined(OS_CHROMEOS)
  // Initialize D-Bus clients that depend on feature list.
  chromeos::InitializeFeatureListDependentDBus();
#endif

#if defined(OS_ANDROID)
  startup_data_->CreateProfilePrefService();
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#endif

  if (base::FeatureList::IsEnabled(
          features::kWriteBasicSystemProfileToPersistentHistogramsFile)) {
    bool record = true;
#if defined(OS_ANDROID)
    record =
        base::FeatureList::IsEnabled(chrome::android::kUmaBackgroundSessions);
#endif
    if (record)
      startup_data_->RecordCoreSystemProfile();
  }
}

bool ChromeMainDelegate::ShouldCreateFeatureList() {
  // Chrome creates the FeatureList, so content should not.
  return false;
}

#endif

void ChromeMainDelegate::PostFieldTrialInitialization() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  bool is_browser_process = process_type.empty();

#if BUILDFLAG(ENABLE_GWP_ASAN_MALLOC)
  {
    version_info::Channel channel = chrome::GetChannel();
    bool is_canary_dev = (channel == version_info::Channel::CANARY ||
                          channel == version_info::Channel::DEV);
    gwp_asan::EnableForMalloc(is_canary_dev || is_browser_process,
                              process_type.c_str());
  }
#endif

#if BUILDFLAG(ENABLE_GWP_ASAN_PARTITIONALLOC)
  {
    version_info::Channel channel = chrome::GetChannel();
    bool is_canary_dev = (channel == version_info::Channel::CANARY ||
                          channel == version_info::Channel::DEV);
    gwp_asan::EnableForPartitionAlloc(is_canary_dev, process_type.c_str());
  }
#endif

  // Start heap profiling as early as possible so it can start recording
  // memory allocations.
  if (is_browser_process) {
    heap_profiler_controller_ = std::make_unique<HeapProfilerController>();
    heap_profiler_controller_->Start();

#if defined(OS_CHROMEOS)
    chromeos::ConfigureSwap();
    chromeos::InitializeKstaled();

    // If we're in an experimental group that locks the browser text we will do
    // that now.
    chromeos::LockMainProgramText();
#endif
  }
}

bool ChromeMainDelegate::BasicStartupComplete(int* exit_code) {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->SaveChromeMainStats();
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Only allow disabling web security via the command-line flag if the user has
  // specified a distinct profile directory. This still enables tests to disable
  // web security by setting the kWebKitWebSecurityEnabled pref directly.
  //
  // Note that this is done in ChromeMainDelegate::BasicStartupComplete()
  // because this is the earliest callback. Many places in Chromium gate
  // security features around kDisableWebSecurity, and it is unreasonable to
  // expect them all to properly also check for kUserDataDir.
  if (command_line.HasSwitch(switches::kDisableWebSecurity)) {
    base::FilePath default_user_data_dir;
    chrome::GetDefaultUserDataDirectory(&default_user_data_dir);
    const base::FilePath specified_user_data_dir =
        command_line.GetSwitchValuePath(switches::kUserDataDir)
            .StripTrailingSeparators();
    if (specified_user_data_dir.empty() ||
        specified_user_data_dir == default_user_data_dir) {
      LOG(ERROR) << "Web security may only be disabled if '--user-data-dir' is "
                    "also specified with a non-default value.";
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kDisableWebSecurity);
    }
  }

#if defined(OS_WIN)
  // Browser should not be sandboxed.
  const bool is_browser = !command_line.HasSwitch(switches::kProcessType);
  if (is_browser && IsSandboxedProcess()) {
    *exit_code = chrome::RESULT_CODE_INVALID_SANDBOX_STATE;
    return true;
  }
#endif

#if defined(OS_MACOSX)
  // Give the browser process a longer treadmill, since crashes
  // there have more impact.
  const bool is_browser = !command_line.HasSwitch(switches::kProcessType);
  ObjcEvilDoers::ZombieEnable(true, is_browser ? 10000 : 1000);
#endif

  content::Profiling::ProcessStarted();

  // Setup tracing sampler profiler as early as possible at startup if needed.
  tracing_sampler_profiler_ =
      tracing::TracingSamplerProfiler::CreateOnMainThread();

#if defined(OS_WIN) && !defined(CHROME_MULTIPLE_DLL_BROWSER)
  v8_crashpad_support::SetUp();
#endif

#if defined(OS_LINUX)
  if (!crash_reporter::IsCrashpadEnabled()) {
    breakpad::SetFirstChanceExceptionHandler(v8::TryHandleWebAssemblyTrapPosix);
  }
#endif

#if defined(OS_POSIX)
  if (HandleVersionSwitches(command_line)) {
    *exit_code = 0;
    return true;  // Got a --version switch; exit with a success error code.
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // This will directly exit if the user asked for help.
  HandleHelpSwitches(command_line);
#endif
#endif  // OS_POSIX

#if defined(OS_WIN)
  // Must do this before any other usage of command line!
  if (HasDeprecatedArguments(command_line.GetCommandLineString())) {
    *exit_code = 1;
    return true;
  }

  if (UseHooks())
    base::debug::InstallHandleHooks();
  else
    base::win::DisableHandleVerifier();

#endif

  chrome::RegisterPathProvider();
#if defined(OS_CHROMEOS)
  chromeos::RegisterPathProvider();
#endif
#if BUILDFLAG(ENABLE_NACL) && defined(OS_LINUX)
  nacl::RegisterPathProvider();
#endif

  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
      kNonWildcardDomainNonPortSchemes, kNonWildcardDomainNonPortSchemesSize);

// No support for ANDROID yet as DiagnosticsController needs wchar support.
// TODO(gspencer): That's not true anymore, or at least there are no w-string
// references anymore. Not sure if that means this can be enabled on Android or
// not though.  As there is no easily accessible command line on Android, I'm
// not sure this is a big deal.
#if !defined(OS_ANDROID) && !defined(CHROME_MULTIPLE_DLL_CHILD)
  // If we are in diagnostics mode this is the end of the line: after the
  // diagnostics are run the process will invariably exit.
  if (command_line.HasSwitch(switches::kDiagnostics)) {
    diagnostics::DiagnosticsWriter::FormatType format =
        diagnostics::DiagnosticsWriter::HUMAN;
    if (command_line.HasSwitch(switches::kDiagnosticsFormat)) {
      std::string format_str =
          command_line.GetSwitchValueASCII(switches::kDiagnosticsFormat);
      if (format_str == "machine") {
        format = diagnostics::DiagnosticsWriter::MACHINE;
      } else if (format_str == "log") {
        format = diagnostics::DiagnosticsWriter::LOG;
      } else {
        DCHECK_EQ("human", format_str);
      }
    }

    diagnostics::DiagnosticsWriter writer(format);
    *exit_code = diagnostics::DiagnosticsController::GetInstance()->Run(
        command_line, &writer);
    diagnostics::DiagnosticsController::GetInstance()->ClearResults();
    return true;
  }
#endif

#if defined(OS_CHROMEOS)
  // Initialize primary user homedir (in multi-profile session) as it may be
  // passed as a command line switch.
  base::FilePath homedir;
  if (command_line.HasSwitch(chromeos::switches::kHomedir)) {
    homedir = base::FilePath(
        command_line.GetSwitchValueASCII(chromeos::switches::kHomedir));
    base::PathService::OverrideAndCreateIfNeeded(base::DIR_HOME, homedir, true,
                                                 false);
  }

  // If we are recovering from a crash on a ChromeOS device, then we will do
  // some recovery using the diagnostics module, and then continue on. We fake
  // up a command line to tell it that we want it to recover, and to preserve
  // the original command line. Note: logging at this point is to /var/log/ui.
  if ((base::SysInfo::IsRunningOnChromeOS() &&
       command_line.HasSwitch(chromeos::switches::kLoginUser)) ||
      command_line.HasSwitch(switches::kDiagnosticsRecovery)) {
    base::CommandLine interim_command_line(command_line.GetProgram());
    const char* const kSwitchNames[] = {switches::kUserDataDir, };
    interim_command_line.CopySwitchesFrom(command_line, kSwitchNames,
                                          base::size(kSwitchNames));
    interim_command_line.AppendSwitch(switches::kDiagnostics);
    interim_command_line.AppendSwitch(switches::kDiagnosticsRecovery);

    diagnostics::DiagnosticsWriter::FormatType format =
        diagnostics::DiagnosticsWriter::LOG;
    if (command_line.HasSwitch(switches::kDiagnosticsFormat)) {
      std::string format_str =
          command_line.GetSwitchValueASCII(switches::kDiagnosticsFormat);
      if (format_str == "machine") {
        format = diagnostics::DiagnosticsWriter::MACHINE;
      } else if (format_str == "human") {
        format = diagnostics::DiagnosticsWriter::HUMAN;
      } else {
        DCHECK_EQ("log", format_str);
      }
    }

    diagnostics::DiagnosticsWriter writer(format);
    int diagnostics_exit_code =
        diagnostics::DiagnosticsController::GetInstance()->Run(command_line,
                                                               &writer);
    if (diagnostics_exit_code) {
      // Diagnostics has failed somehow, so we exit.
      *exit_code = diagnostics_exit_code;
      return true;
    }

    // Now we run the actual recovery tasks.
    int recovery_exit_code =
        diagnostics::DiagnosticsController::GetInstance()->RunRecovery(
            command_line, &writer);

    if (recovery_exit_code) {
      // Recovery has failed somehow, so we exit.
      *exit_code = recovery_exit_code;
      return true;
    }
  } else {  // Not running diagnostics or recovery.
    diagnostics::DiagnosticsController::GetInstance()->RecordRegularStartup();
  }
#endif

  content::SetContentClient(&chrome_content_client_);

  // The TLS slot used by the memlog allocator shim needs to be initialized
  // early to ensure that it gets assigned a low slot number. If it gets
  // initialized too late, the glibc TLS system will require a malloc call in
  // order to allocate storage for a higher slot number. Since malloc is hooked,
  // this causes re-entrancy into the allocator shim, while the TLS object is
  // partially-initialized, which the TLS object is supposed to protect again.
  heap_profiling::InitTLSSlot();

  return false;
}

#if defined(OS_MACOSX)
void ChromeMainDelegate::InitMacCrashReporter(
    const base::CommandLine& command_line,
    const std::string& process_type) {
  // TODO(mark): Right now, InitializeCrashpad() needs to be called after
  // CommandLine::Init() and chrome::RegisterPathProvider().  Ideally, Crashpad
  // initialization could occur sooner, preferably even before the framework
  // dylib is even loaded, to catch potential early crashes.

  const bool browser_process = process_type.empty();
  const bool install_from_dmg_relauncher_process =
      process_type == switches::kRelauncherProcess &&
      command_line.HasSwitch(switches::kRelauncherProcessDMGDevice);

  const bool initial_client =
      browser_process || install_from_dmg_relauncher_process;

  crash_reporter::InitializeCrashpad(initial_client, process_type);

  if (!browser_process) {
    std::string metrics_client_id =
        command_line.GetSwitchValueASCII(switches::kMetricsClientID);
    crash_keys::SetMetricsClientIdFromGUID(metrics_client_id);
  }

  // Mac Chrome is packaged with a main app bundle and a helper app bundle.
  // The main app bundle should only be used for the browser process, so it
  // should never see a --type switch (switches::kProcessType).  Likewise,
  // the helper should always have a --type switch.
  //
  // This check is done this late so there is already a call to
  // base::mac::IsBackgroundOnlyProcess(), so there is no change in
  // startup/initialization order.

  // The helper's Info.plist marks it as a background only app.
  if (base::mac::IsBackgroundOnlyProcess()) {
    CHECK(command_line.HasSwitch(switches::kProcessType) &&
          !process_type.empty())
        << "Helper application requires --type.";
  } else if (base::mac::AmIBundled()) {
    CHECK(!command_line.HasSwitch(switches::kProcessType) &&
          process_type.empty())
        << "Main application forbids --type, saw " << process_type;
  }
}

void ChromeMainDelegate::SetUpInstallerPreferences(
    const base::CommandLine& command_line) {
  const bool uma_setting = command_line.HasSwitch(switches::kEnableUserMetrics);
  const bool default_browser_setting =
      command_line.HasSwitch(switches::kMakeChromeDefault);

  if (uma_setting)
    crash_reporter::SetUploadConsent(uma_setting);
  if (default_browser_setting)
    shell_integration::SetAsDefaultBrowser();
}
#endif  // defined(OS_MACOSX)

void ChromeMainDelegate::PreSandboxStartup() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  crash_reporter::InitializeCrashKeys();

#if defined(OS_POSIX)
  ChromeCrashReporterClient::Create();
#endif

#if defined(OS_MACOSX)
  InitMacCrashReporter(command_line, process_type);
  SetUpInstallerPreferences(command_line);
#endif

#if defined(OS_WIN)
  child_process_logging::Init();
#endif
#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

  // Initialize the user data dir for any process type that needs it.
  if (chrome::ProcessNeedsProfileDir(process_type))
    InitializeUserDataDir(base::CommandLine::ForCurrentProcess());

  // Register component_updater PathProvider after DIR_USER_DATA overidden by
  // command line flags. Maybe move the chrome PathProvider down here also?
  int alt_preinstalled_components_dir =
#if defined(OS_CHROMEOS)
      chromeos::DIR_PREINSTALLED_COMPONENTS;
#else
      chrome::DIR_INTERNAL_PLUGINS;
#endif
  component_updater::RegisterPathProvider(chrome::DIR_COMPONENTS,
                                          alt_preinstalled_components_dir,
                                          chrome::DIR_USER_DATA);

#if !defined(OS_ANDROID) && !defined(OS_WIN)
  // Android does InitLogging when library is loaded. Skip here.
  // For windows we call InitLogging when the sandbox is initialized.
  InitLogging(process_type);
#endif

#if defined(OS_WIN)
  // TODO(zturner): Throbber icons are still stored in chrome.dll, this can be
  // killed once those are merged into resources.pak.  See
  // GlassBrowserFrameView::InitThrobberIcons() and http://crbug.com/368327.
  ui::SetResourcesDataDLL(_AtlBaseModule.GetResourceInstance());
#endif

  if (SubprocessNeedsResourceBundle(process_type)) {
    // Initialize ResourceBundle which handles files loaded from external
    // sources.  The language should have been passed in to us from the
    // browser process as a command line flag.
#if !BUILDFLAG(ENABLE_NACL)
    DCHECK(command_line.HasSwitch(switches::kLang) ||
           process_type == service_manager::switches::kZygoteProcess ||
           process_type == switches::kGpuProcess ||
           process_type == switches::kPpapiBrokerProcess ||
           process_type == switches::kPpapiPluginProcess);
#else
    DCHECK(command_line.HasSwitch(switches::kLang) ||
           process_type == service_manager::switches::kZygoteProcess ||
           process_type == switches::kGpuProcess ||
           process_type == switches::kNaClLoaderProcess ||
           process_type == switches::kPpapiBrokerProcess ||
           process_type == switches::kPpapiPluginProcess);
#endif

    // TODO(markusheintz): The command line flag --lang is actually processed
    // by the CommandLinePrefStore, and made available through the PrefService
    // via the preference prefs::kApplicationLocale. The browser process uses
    // the --lang flag to pass the value of the PrefService in here. Maybe
    // this value could be passed in a different way.
    std::string locale = command_line.GetSwitchValueASCII(switches::kLang);
#if defined(OS_CHROMEOS)
    if (process_type == service_manager::switches::kZygoteProcess) {
      DCHECK(locale.empty());
      // See comment at ReadAppLocale() for why we do this.
      locale = chromeos::startup_settings_cache::ReadAppLocale();
    }
#endif
#if defined(OS_ANDROID)
    // The renderer sandbox prevents us from accessing our .pak files directly.
    // Therefore file descriptors to the .pak files that we need are passed in
    // at process creation time.
    auto* global_descriptors = base::GlobalDescriptors::GetInstance();
    int pak_fd = global_descriptors->Get(kAndroidLocalePakDescriptor);
    base::MemoryMappedFile::Region pak_region =
        global_descriptors->GetRegion(kAndroidLocalePakDescriptor);
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                            pak_region);

    // Load secondary locale .pak file if it exists.
    pak_fd = global_descriptors->MaybeGet(kAndroidSecondaryLocalePakDescriptor);
    if (pak_fd != -1) {
      pak_region = global_descriptors->GetRegion(
          kAndroidSecondaryLocalePakDescriptor);
      ui::ResourceBundle::GetSharedInstance()
          .LoadSecondaryLocaleDataWithPakFileRegion(base::File(pak_fd),
                                                    pak_region);
    }

    int extra_pak_keys[] = {
      kAndroidChrome100PercentPakDescriptor,
      kAndroidUIResourcesPakDescriptor,
    };
    for (int extra_pak_key : extra_pak_keys) {
      pak_fd = global_descriptors->Get(extra_pak_key);
      pak_region = global_descriptors->GetRegion(extra_pak_key);
      ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
          base::File(pak_fd), pak_region, ui::SCALE_FACTOR_100P);
    }

    // For Android: Native resources for DFMs should only be used by the browser
    // process. Their file descriptors and memory mapped file region are not
    // passed to child processes, and are therefore not loaded here.

    base::i18n::SetICUDefaultLocale(locale);
    const std::string loaded_locale = locale;
#else
    ui::MaterialDesignController::Initialize();
    const std::string loaded_locale =
        ui::ResourceBundle::InitSharedInstanceWithLocale(
            locale, NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

    base::FilePath resources_pack_path;
    base::PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path, ui::SCALE_FACTOR_NONE);
#endif
    CHECK(!loaded_locale.empty()) << "Locale could not be found for " <<
        locale;
  }

#if !defined(CHROME_MULTIPLE_DLL_BROWSER) && BUILDFLAG(ENABLE_PDF)
  InitializePDF();
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Zygote needs to call InitCrashReporter() in RunZygote().
  if (process_type != service_manager::switches::kZygoteProcess) {
#if defined(OS_ANDROID)
    crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
    if (process_type.empty()) {
      base::android::InitJavaExceptionReporter();
      UninstallPureJavaExceptionHandler();
    } else {
      base::android::InitJavaExceptionReporterForChildProcess();
    }
#else  // !defined(OS_ANDROID)
    if (crash_reporter::IsCrashpadEnabled()) {
      crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
      crash_reporter::SetFirstChanceExceptionHandler(
          v8::TryHandleWebAssemblyTrapPosix);
    } else {
      breakpad::InitCrashReporter(process_type);
    }
#endif  // defined(OS_ANDROID)
  }
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

  // After all the platform Breakpads have been initialized, store the command
  // line for crash reporting.
  crash_keys::SetCrashKeysFromCommandLine(command_line);
}

void ChromeMainDelegate::SandboxInitialized(const std::string& process_type) {
  // Note: If you are adding a new process type below, be sure to adjust the
  // AdjustLinuxOOMScore function too.
#if defined(OS_LINUX)
  AdjustLinuxOOMScore(process_type);
#endif
#if defined(OS_WIN)
  InitLogging(process_type);
  SuppressWindowsErrorDialogs();
#endif

#if defined(CHROME_MULTIPLE_DLL_CHILD) || !defined(CHROME_MULTIPLE_DLL_BROWSER)
#if BUILDFLAG(ENABLE_NACL)
  ChromeContentClient::SetNaClEntryFunctions(
      nacl_plugin::PPP_GetInterface,
      nacl_plugin::PPP_InitializeModule,
      nacl_plugin::PPP_ShutdownModule);
#endif
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_PDF)
  ChromeContentClient::SetPDFEntryFunctions(
      chrome_pdf::PPP_GetInterface,
      chrome_pdf::PPP_InitializeModule,
      chrome_pdf::PPP_ShutdownModule);
#endif
#endif
}

int ChromeMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
// ANDROID doesn't support "service", so no CloudPrintServiceProcessMain, and
// arraysize doesn't support empty array. So we comment out the block for
// Android.
#if defined(OS_ANDROID)
  NOTREACHED();  // Android provides a subclass and shares no code here.
#else
  static const MainFunction kMainFunctions[] = {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(CHROME_MULTIPLE_DLL_CHILD) && \
    !defined(OS_CHROMEOS)
    {switches::kCloudPrintServiceProcess, CloudPrintServiceProcessMain},
#endif

#if defined(OS_MACOSX)
    {switches::kRelauncherProcess, mac_relauncher::internal::RelauncherMain},
#endif

    // This entry is not needed on Linux, where the NaCl loader
    // process is launched via nacl_helper instead.
#if BUILDFLAG(ENABLE_NACL) && !defined(CHROME_MULTIPLE_DLL_BROWSER) && \
    !defined(OS_LINUX)
    {switches::kNaClLoaderProcess, NaClMain},
#else
    {"<invalid>", NULL},  // To avoid constant array of size 0
                          // when NaCl disabled and CHROME_MULTIPLE_DLL_CHILD
#endif
  };

  for (size_t i = 0; i < base::size(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_function_params);
  }
#endif  // !defined(OS_ANDROID)

  return -1;
}

void ChromeMainDelegate::ProcessExiting(const std::string& process_type) {
  if (SubprocessNeedsResourceBundle(process_type))
    ui::ResourceBundle::CleanupSharedInstance();
#if !defined(OS_ANDROID)
  logging::CleanupChromeLogging();
#else
  // Android doesn't use InitChromeLogging, so we close the log file manually.
  logging::CloseLogFile();
#endif  // !defined(OS_ANDROID)
}

#if defined(OS_MACOSX)
bool ChromeMainDelegate::ProcessRegistersWithSystemProcess(
    const std::string& process_type) {
#if !BUILDFLAG(ENABLE_NACL)
  return false;
#else
  return process_type == switches::kNaClLoaderProcess;
#endif
}

bool ChromeMainDelegate::DelaySandboxInitialization(
    const std::string& process_type) {
#if BUILDFLAG(ENABLE_NACL)
  // NaClLoader does this in NaClMainPlatformDelegate::EnableSandbox().
  // No sandbox needed for relauncher.
  if (process_type == switches::kNaClLoaderProcess)
    return true;
#endif
  return process_type == switches::kRelauncherProcess;
}
#elif defined(OS_LINUX)
void ChromeMainDelegate::ZygoteStarting(
    std::vector<std::unique_ptr<service_manager::ZygoteForkDelegate>>*
        delegates) {
#if defined(OS_CHROMEOS)
    chromeos::InitHugepagesAndMlockSelf();
#endif

#if BUILDFLAG(ENABLE_NACL)
    nacl::AddNaClZygoteForkDelegates(delegates);
#endif
}

void ChromeMainDelegate::ZygoteForked() {
  content::Profiling::ProcessStarted();
  if (content::Profiling::BeingProfiled()) {
    base::debug::RestartProfilingAfterFork();
    SetUpProfilingShutdownHandler();
  }

  // Needs to be called after we have chrome::DIR_USER_DATA.  BrowserMain sets
  // this up for the browser process in a different manner.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (crash_reporter::IsCrashpadEnabled()) {
    crash_reporter::InitializeCrashpad(false, process_type);
    crash_reporter::SetFirstChanceExceptionHandler(
        v8::TryHandleWebAssemblyTrapPosix);
  } else {
    breakpad::InitCrashReporter(process_type);
  }

  // Reset the command line for the newly spawned process.
  crash_keys::SetCrashKeysFromCommandLine(*command_line);
}

#endif  // defined(OS_LINUX)

content::ContentBrowserClient*
ChromeMainDelegate::CreateContentBrowserClient() {
#if defined(CHROME_MULTIPLE_DLL_CHILD)
  return NULL;
#else
  if (chrome_content_browser_client_ == nullptr) {
    DCHECK(!startup_data_);
    startup_data_ = std::make_unique<StartupData>();

    chrome_content_browser_client_ =
        std::make_unique<ChromeContentBrowserClient>(startup_data_.get());
  }
  return chrome_content_browser_client_.get();
#endif
}

content::ContentGpuClient* ChromeMainDelegate::CreateContentGpuClient() {
#if defined(CHROME_MULTIPLE_DLL_BROWSER)
  return nullptr;
#else
  return g_chrome_content_gpu_client.Pointer();
#endif
}

content::ContentRendererClient*
ChromeMainDelegate::CreateContentRendererClient() {
#if defined(CHROME_MULTIPLE_DLL_BROWSER)
  return NULL;
#else
  return g_chrome_content_renderer_client.Pointer();
#endif
}

content::ContentUtilityClient*
ChromeMainDelegate::CreateContentUtilityClient() {
#if defined(CHROME_MULTIPLE_DLL_BROWSER)
  return NULL;
#else
  return g_chrome_content_utility_client.Pointer();
#endif
}

service_manager::ProcessType ChromeMainDelegate::OverrideProcessType() {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.GetSwitchValueASCII(switches::kProcessType) ==
      service_manager::switches::kProcessTypeService) {
    // Don't mess with embedded service command lines.
    return service_manager::ProcessType::kDefault;
  }
  return service_manager::ProcessType::kDefault;
}

void ChromeMainDelegate::PreCreateMainMessageLoop() {
#if defined(OS_MACOSX)
  // Tell Cocoa to finish its initialization, which we want to do manually
  // instead of calling NSApplicationMain(). The primary reason is that NSAM()
  // never returns, which would leave all the objects currently on the stack
  // in scoped_ptrs hanging and never cleaned up. We then load the main nib
  // directly. The main event loop is run from common code using the
  // MessageLoop API, which works out ok for us because it's a wrapper around
  // CFRunLoop.

  // Initialize NSApplication using the custom subclass.
  chrome_browser_application_mac::RegisterBrowserCrApp();

  if (l10n_util::GetLocaleOverride().empty()) {
    // The browser process only wants to support the language Cocoa will use,
    // so force the app locale to be overridden with that value. This must
    // happen before the ResourceBundle is loaded, which happens in
    // ChromeBrowserMainParts::PreEarlyInitialization().
    // Don't do this if the locale is already set, which is done by integration
    // tests to ensure tests always run with the same locale.
    l10n_util::OverrideLocaleWithCocoaLocale();
  }
#endif
}
