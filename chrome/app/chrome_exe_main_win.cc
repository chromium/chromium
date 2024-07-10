// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_exe_main_win.h"

#include <tchar.h>
#include <windows.h>

#include <malloc.h>
#include <stddef.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/handle_hooks_win.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/current_module.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/app/delay_load_failure_hook_win.h"
#include "chrome/app/exit_code_watcher_win.h"
#include "chrome/app/main_dll_loader_win.h"
#include "chrome/app/packed_resources_integrity.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/browser/win/chrome_process_finder.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/user_data_dir.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "components/crash/core/app/fallback_crash_handling_win.h"
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "third_party/crashpad/crashpad/util/win/initial_client_data.h"

#if defined(WIN_CONSOLE_APP)
// Forward declaration of main.
int main();
#endif

namespace {

// Sets the current working directory for the process to the directory holding
// the executable if this is the browser process. This avoids leaking a handle
// to an arbitrary directory to child processes (e.g., the crashpad handler
// process) created before MainDllLoader changes the current working directory
// to the browser's version directory.
void SetCwdForBrowserProcess() {
  if (!::IsBrowserProcess())
    return;

  std::array<wchar_t, MAX_PATH + 1> buffer;
  buffer[0] = L'\0';
  DWORD length = ::GetModuleFileName(nullptr, &buffer[0], buffer.size());
  if (!length || length >= buffer.size())
    return;

  base::SetCurrentDirectory(
      base::FilePath(base::FilePath::StringPieceType(&buffer[0], length))
          .DirName());
}

bool IsFastStartSwitch(const std::string& command_line_switch) {
  return command_line_switch == switches::kProfileDirectory;
}

bool ContainsNonFastStartFlag(const base::CommandLine& command_line) {
  const base::CommandLine::SwitchMap& switches = command_line.GetSwitches();
  if (switches.size() > 1)
    return true;
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (!IsFastStartSwitch(it->first))
      return true;
  }
  return false;
}

bool AttemptFastNotify(const base::CommandLine& command_line) {
  if (ContainsNonFastStartFlag(command_line))
    return false;

  base::FilePath user_data_dir;
  if (!chrome::GetDefaultUserDataDirectory(&user_data_dir))
    return false;
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);

  HWND chrome = chrome::FindRunningChromeWindow(user_data_dir);
  if (!chrome)
    return false;
  return chrome::AttemptToNotifyRunningChrome(chrome) == chrome::NOTIFY_SUCCESS;
}

// Returns true if the child process |command_line| contains a /prefetch:#
// argument where # is in [1, 8] prior to Win11 and [1,16] for it and later.
// The intent of the function is to ensure that all child processes have a
// /prefetch:N cmd line arg in the required range.
// No child process shall have /prefetch:0 or it will interefere with the main
// browser process prefetch. This includes things like /prefetch:simians where
// simians will evalate to 0. Absence of a /prefetch:N argument is the same as
// /prefetch:0 and is also excluded.
// The function assumes only one /prefetch:N argument for child processes.
bool HasValidWindowsPrefetchArgument(const base::CommandLine& command_line) {
  static constexpr std::wstring_view kPrefetchArgumentPrefix(L"/prefetch:");

  for (const auto& arg : command_line.argv()) {
    if (!base::StartsWith(arg, kPrefetchArgumentPrefix)) {
      continue;  // Ignore arguments that don't start with "/prefetch:".
    }
    auto value = std::wstring_view(arg).substr(kPrefetchArgumentPrefix.size());
    int profile = 0;
    return base::StringToInt(value, &profile) && profile >= 1 &&
           profile <=
               (base::win::GetVersion() < base::win::Version::WIN11 ? 8 : 16);
  }
  return false;
}

int RunFallbackCrashHandler(const base::CommandLine& cmd_line) {
  // Retrieve the product & version details we need to report the crash
  // correctly.
  wchar_t exe_file[MAX_PATH] = {};
  CHECK(::GetModuleFileName(nullptr, exe_file, std::size(exe_file)));

  std::wstring product_name, version, channel_name, special_build;
  install_static::GetExecutableVersionDetails(exe_file, &product_name, &version,
                                              &special_build, &channel_name);

  return crash_reporter::RunAsFallbackCrashHandler(
      cmd_line, base::WideToUTF8(product_name), base::WideToUTF8(version),
      base::WideToUTF8(channel_name));
}

// In 32-bit builds, the main thread starts with the default (small) stack size.
// The ARCH_CPU_32_BITS blocks here and below are in support of moving the main
// thread to a fiber with a larger stack size.
#if defined(ARCH_CPU_32_BITS)
// The information needed to transfer control to the large-stack fiber and later
// pass the main routine's exit code back to the small-stack fiber prior to
// termination.
struct FiberState {
  HINSTANCE instance;
  LPVOID original_fiber;
  int fiber_result;
};

// A PFIBER_START_ROUTINE function run on a large-stack fiber that calls the
// main routine, stores its return value, and returns control to the small-stack
// fiber. |params| must be a pointer to a FiberState struct.
void WINAPI FiberBinder(void* params) {
  auto* fiber_state = static_cast<FiberState*>(params);
  // Call the main routine from the fiber. Reusing the entry point minimizes
  // confusion when examining call stacks in crash reports - seeing wWinMain on
  // the stack is a handy hint that this is the main thread of the process.
#if !defined(WIN_CONSOLE_APP)
  fiber_state->fiber_result =
      wWinMain(fiber_state->instance, nullptr, nullptr, 0);
#else   // !defined(WIN_CONSOLE_APP)
  fiber_state->fiber_result = main();
#endif  // !defined(WIN_CONSOLE_APP)
  // Switch back to the main thread to exit.
  ::SwitchToFiber(fiber_state->original_fiber);
}
#endif  // defined(ARCH_CPU_32_BITS)

}  // namespace

__declspec(dllexport) __cdecl void GetPakFileHashes(
    const uint8_t** resources_pak,
    const uint8_t** chrome_100_pak,
    const uint8_t** chrome_200_pak) {
  *resources_pak = kSha256_resources_pak.data();
  *chrome_100_pak = kSha256_chrome_100_percent_pak.data();
  *chrome_200_pak = kSha256_chrome_200_percent_pak.data();
}

#if !defined(WIN_CONSOLE_APP)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
#else   // !defined(WIN_CONSOLE_APP)
int main() {
  HINSTANCE instance = GetModuleHandle(nullptr);
#endif  // !defined(WIN_CONSOLE_APP)

#if defined(ARCH_CPU_32_BITS)
  enum class FiberStatus { kConvertFailed, kCreateFiberFailed, kSuccess };
  FiberStatus fiber_status = FiberStatus::kSuccess;
  // GetLastError result if fiber conversion failed.
  DWORD fiber_error = ERROR_SUCCESS;
  if (!::IsThreadAFiber()) {
    // Make the main thread's stack size 4 MiB so that it has roughly the same
    // effective size as the 64-bit build's 8 MiB stack.
    constexpr size_t kStackSize = 4 * 1024 * 1024;  // 4 MiB
    // Leak the fiber on exit.
    LPVOID original_fiber =
        ::ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
    if (original_fiber) {
      FiberState fiber_state = {instance, original_fiber};
      // Create a fiber with a bigger stack and switch to it. Leak the fiber on
      // exit.
      LPVOID big_stack_fiber = ::CreateFiberEx(
          0, kStackSize, FIBER_FLAG_FLOAT_SWITCH, FiberBinder, &fiber_state);
      if (big_stack_fiber) {
        ::SwitchToFiber(big_stack_fiber);
        // The fibers must be cleaned up to avoid obscure TLS-related shutdown
        // crashes.
        ::DeleteFiber(big_stack_fiber);
        ::ConvertFiberToThread();
        // Control returns here after Chrome has finished running on FiberMain.
        return fiber_state.fiber_result;
      }
      fiber_status = FiberStatus::kCreateFiberFailed;
    } else {
      fiber_status = FiberStatus::kConvertFailed;
    }
    // If we reach here then creating and switching to a fiber has failed. This
    // probably means we are low on memory and will soon crash. Try to report
    // this error once crash reporting is initialized.
    fiber_error = ::GetLastError();
    base::debug::Alias(&fiber_error);
  }
  // If we are already a fiber then continue normal execution.
#endif  // defined(ARCH_CPU_32_BITS)

  SetCwdForBrowserProcess();
  install_static::InitializeFromPrimaryModule();
  SignalInitializeCrashReporting();
  if (IsBrowserProcess())
    chrome::DisableDelayLoadFailureHooksForMainExecutable();
#if defined(ARCH_CPU_32_BITS)
  // Intentionally crash if converting to a fiber failed.
  CHECK_EQ(fiber_status, FiberStatus::kSuccess);
#endif  // defined(ARCH_CPU_32_BITS)

  // Done here to ensure that OOMs that happen early in process initialization
  // are correctly signaled to the OS.
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();

  // Initialize the CommandLine singleton from the environment.
  base::CommandLine::Init(0, nullptr);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  const std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if !defined(COMPONENT_BUILD) && DCHECK_IS_ON()
  // In non-component mode, chrome.exe contains its own base::FeatureList
  // instance pointer, which remains nullptr. Attempts to access feature state
  // from chrome.exe should fail, instead of silently returning a default state.
  base::FeatureList::FailOnFeatureAccessWithoutFeatureList();

  // Patch the main EXE on non-component builds when DCHECKs are enabled.
  // This allows detection of third party code that might attempt to meddle with
  // Chrome's handles. This must be done when single-threaded to avoid other
  // threads attempting to make calls through the hooks while they are being
  // emplaced.
  // Note: The DLL is patched separately, in chrome/app/chrome_main.cc.
  base::debug::HandleHooks::AddIATPatch(CURRENT_MODULE());
#endif  // !defined(COMPONENT_BUILD) && DCHECK_IS_ON()

  // Confirm that an explicit prefetch profile is used for all process types
  // except for the browser process. Any new process type will have to assign
  // itself a prefetch id. See kPrefetchArgument* constants in
  // content_switches.cc for details.
  DCHECK(process_type.empty() ||
         HasValidWindowsPrefetchArgument(*command_line));

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    // Check if we should monitor the exit code of this process
    std::unique_ptr<ExitCodeWatcher> exit_code_watcher;

    crash_reporter::SetupFallbackCrashHandling(*command_line);
    // no-periodic-tasks is specified for self monitoring crashpad instances.
    // This is to ensure we are a crashpad process monitoring the browser
    // process and not another crashpad process.
    if (!command_line->HasSwitch("no-periodic-tasks")) {
      // Retrieve the client process from the command line
      crashpad::InitialClientData initial_client_data;
      if (initial_client_data.InitializeFromString(
              command_line->GetSwitchValueASCII("initial-client-data"))) {
        // Setup exit code watcher to monitor the parent process
        HANDLE duplicate_handle = INVALID_HANDLE_VALUE;
        if (DuplicateHandle(
                ::GetCurrentProcess(), initial_client_data.client_process(),
                ::GetCurrentProcess(), &duplicate_handle,
                PROCESS_QUERY_INFORMATION, FALSE, DUPLICATE_SAME_ACCESS)) {
          base::Process parent_process(duplicate_handle);
          exit_code_watcher = std::make_unique<ExitCodeWatcher>();
          if (exit_code_watcher->Initialize(std::move(parent_process))) {
            exit_code_watcher->StartWatching();
          }
        }
      }
    }

    // The handler process must always be passed the user data dir on the
    // command line.
    DCHECK(command_line->HasSwitch(switches::kUserDataDir));

    base::FilePath user_data_dir =
        command_line->GetSwitchValuePath(switches::kUserDataDir);
    int crashpad_status = crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess(), user_data_dir,
        switches::kProcessType, switches::kUserDataDir);
    if (crashpad_status != 0 && exit_code_watcher) {
      // Crashpad failed to initialize, explicitly stop the exit code watcher
      // so the crashpad-handler process can exit with an error
      exit_code_watcher->StopWatching();
    }
    return crashpad_status;
  } else if (process_type == crash_reporter::switches::kFallbackCrashHandler) {
    return RunFallbackCrashHandler(*command_line);
  }

  const base::TimeTicks exe_entry_point_ticks = base::TimeTicks::Now();

  // Signal Chrome Elf that Chrome has begun to start.
  SignalChromeElf();

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  if (AttemptFastNotify(*command_line))
    return 0;

  // Load and launch the chrome dll. *Everything* happens inside.
  VLOG(1) << "About to load main DLL.";
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance, exe_entry_point_ticks);
  loader->RelaunchChromeBrowserWithNewCommandLineIfNeeded();
  delete loader;

  // Process shutdown is hard and some process types have been crashing during
  // shutdown. TerminateCurrentProcessImmediately is safer and faster.
  if (process_type == switches::kUtilityProcess ||
      process_type == switches::kPpapiPluginProcess) {
    base::Process::TerminateCurrentProcessImmediately(rc);
  }
  return rc;
}
