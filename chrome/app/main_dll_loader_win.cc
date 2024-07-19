// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/main_dll_loader_win.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <userenv.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "base/win/shlwapi.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/active_use_util.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/update_did_run_state.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/app/sandbox_helper_win.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/win/src/sandbox.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE,
                        sandbox::SandboxInterfaceInfo*,
                        int64_t exe_main_entry_point_ticks,
                        int64_t preread_begin_ticks,
                        int64_t preread_end_ticks);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

void RecordDidRun(const base::FilePath& dll_path) {
  installer::UpdateDidRunState();
}

// Indicates whether a file can be opened using the same flags that
// ::LoadLibrary() uses to open modules.
bool ModuleCanBeRead(const base::FilePath& file_path) {
  return base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ)
      .IsValid();
}

// Returns the full path to |module_name|. Both dev builds (where |module_name|
// is in the current executable's directory) and proper installs (where
// |module_name| is in a versioned sub-directory of the current executable's
// directory) are supported. The identified file is not guaranteed to exist.
base::FilePath GetModulePath(std::wstring_view module_name) {
  base::FilePath exe_dir;
  const bool has_path = base::PathService::Get(base::DIR_EXE, &exe_dir);
  DCHECK(has_path);

  // Look for the module in a versioned sub-directory of the current
  // executable's directory and return the path if it can be read. This is the
  // expected location of modules for proper installs.
  const base::FilePath module_path =
      exe_dir.AppendASCII(chrome::kChromeVersion).Append(module_name);
  if (ModuleCanBeRead(module_path))
    return module_path;

  // Othwerwise, return the path to the module in the current executable's
  // directory. This is the expected location of modules for dev builds.
  return exe_dir.Append(module_name);
}

// Prefetches and loads |module| after setting the CWD to |module|'s
// directory. Returns a handle to the loaded module on success, or nullptr on
// failure.
HMODULE LoadModuleWithDirectory(const base::FilePath& module,
                                const base::CommandLine& cmd_line,
                                bool is_browser,
                                base::TimeTicks& preread_begin_ticks,
                                base::TimeTicks& preread_end_ticks) {
  ::SetCurrentDirectoryW(module.DirName().value().c_str());
  if (is_browser) {
    preread_begin_ticks = base::TimeTicks::Now();
    // Always call PreReadFile() for the main browser process.
    base::PreReadFile(module, /*is_executable=*/true, /*sequential=*/false);
    preread_end_ticks = base::TimeTicks::Now();
  } else {
    // The kNoPreReadMainDll experiment only impacts other processes. Isolate
    // the check so the experiment is easier to remove later if we land the
    // PrefetchVirtualMemoryPolicy experiment.
    if (!cmd_line.HasSwitch(switches::kNoPreReadMainDll)) {
      preread_begin_ticks = base::TimeTicks::Now();
      base::PreReadFile(module, /*is_executable=*/true, /*sequential=*/false);
      preread_end_ticks = base::TimeTicks::Now();
    }
  }
  HMODULE handle = ::LoadLibraryExW(module.value().c_str(), nullptr,
                                    LOAD_WITH_ALTERED_SEARCH_PATH);
  return handle;
}

// Prefetches and loads the appropriate DLL for the process type
// |process_type_|. Populates |module| with the path of the loaded DLL.
// Returns a handle to the loaded DLL, or nullptr on failure.
HMODULE Load(base::FilePath* module,
             const base::CommandLine& cmd_line,
             bool is_browser,
             base::TimeTicks& preread_begin_ticks,
             base::TimeTicks& preread_end_ticks) {
  *module = GetModulePath(installer::kChromeDll);
  if (module->empty()) {
    PLOG(ERROR) << "Cannot find module " << installer::kChromeDll;
    return nullptr;
  }
  HMODULE dll = LoadModuleWithDirectory(*module, cmd_line, is_browser,
                                        preread_begin_ticks, preread_end_ticks);
  if (!dll) {
    PLOG(ERROR) << "Failed to load Chrome DLL from " << module->value();
  }
  return dll;
}

}  // namespace

//=============================================================================

MainDllLoader::MainDllLoader() : dll_(nullptr) {}

MainDllLoader::~MainDllLoader() = default;

const int kNonBrowserShutdownPriority = 0x280;

// Launching is a matter of loading the right dll and calling the entry point.
// Derived classes can add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance,
                          base::TimeTicks exe_entry_point_ticks) {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  process_type_ = cmd_line.GetSwitchValueASCII(switches::kProcessType);

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  const bool is_browser = process_type_.empty();
  // IsUnsandboxedSandboxType() can't be used here because its result can be
  // gated behind a feature flag, which are not yet initialized.
  const bool is_sandboxed =
      sandbox::policy::SandboxTypeFromCommandLine(cmd_line) !=
      sandbox::mojom::Sandbox::kNoSandbox;

  if (is_browser || is_sandboxed) {
    // For child processes that are running as --no-sandbox, don't initialize
    // the sandbox info, otherwise they'll be treated as brokers (as if they
    // were the browser).
    content::InitializeSandboxInfo(
        &sandbox_info, IsExtensionPointDisableSet()
                           ? sandbox::MITIGATION_EXTENSION_POINT_DISABLE
                           : 0);
  }

  base::TimeTicks preread_begin_ticks;
  base::TimeTicks preread_end_ticks;

  base::FilePath file;
  dll_ =
      Load(&file, cmd_line, is_browser, preread_begin_ticks, preread_end_ticks);
  if (!dll_)
    return chrome::RESULT_CODE_MISSING_DATA;

  if (!is_browser) {
    // Set non-browser processes up to be killed by the system after the
    // browser goes away. The browser uses the default shutdown order, which
    // is 0x280. Note that lower numbers here denote "kill later" and higher
    // numbers mean "kill sooner". This gets rid of most of those unsightly
    // sad tabs on logout and shutdown.
    ::SetProcessShutdownParameters(kNonBrowserShutdownPriority - 1,
                                   SHUTDOWN_NORETRY);
  }

  OnBeforeLaunch(process_type_, file);
  DLL_MAIN chrome_main =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  int rc = chrome_main(instance, &sandbox_info,
                       exe_entry_point_ticks.ToInternalValue(),
                       preread_begin_ticks.ToInternalValue(),
                       preread_end_ticks.ToInternalValue());
  return rc;
}

void MainDllLoader::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  // The relaunch-if-needed behavior is a NOP for processes other than the
  // browser process, so early out here.
  if (!dll_ || !process_type_.empty())
    return;

  RelaunchChromeBrowserWithNewCommandLineIfNeededFunc relaunch_function =
      reinterpret_cast<RelaunchChromeBrowserWithNewCommandLineIfNeededFunc>(
          ::GetProcAddress(dll_,
                           "RelaunchChromeBrowserWithNewCommandLineIfNeeded"));
  CHECK(relaunch_function);
  relaunch_function();
}

//=============================================================================

class ChromeDllLoader : public MainDllLoader {
 protected:
  // MainDllLoader implementation.
  void OnBeforeLaunch(const std::string& process_type,
                      const base::FilePath& dll_path) override;
};

void ChromeDllLoader::OnBeforeLaunch(const std::string& process_type,
                                     const base::FilePath& dll_path) {
  if (process_type.empty()) {
    if constexpr (kShouldRecordActiveUse) {
      RecordDidRun(dll_path);
    }
  } else {
    // Set non-browser processes up to be killed by the system after the browser
    // goes away. The browser uses the default shutdown order, which is 0x280.
    // Note that lower numbers here denote "kill later" and higher numbers mean
    // "kill sooner".
    // This gets rid of most of those unsightly sad tabs on logout and shutdown.
    ::SetProcessShutdownParameters(kNonBrowserShutdownPriority - 1,
                                   SHUTDOWN_NORETRY);
  }
}

//=============================================================================

MainDllLoader* MakeMainDllLoader() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return new ChromeDllLoader();
#else
  return new MainDllLoader();
#endif
}
