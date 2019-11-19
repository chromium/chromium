// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/main_dll_loader_win.h"

#include <windows.h>  // NOLINT
#include <stddef.h>
#include <stdint.h>
#include <userenv.h>  // NOLINT

#include <memory>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "base/win/shlwapi.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_watcher_client_win.h"
#include "chrome/app/chrome_watcher_command_line_win.h"
#include "chrome/browser/active_use_util.h"
#include "chrome/chrome_watcher/chrome_watcher_main_api.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics_constants_util_win.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/app/sandbox_helper_win.h"
#include "content/public/common/content_switches.h"
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/switches.h"

namespace {
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*, int64_t);

typedef void (*RelaunchChromeBrowserWithNewCommandLineIfNeededFunc)();

// Loads |module| after setting the CWD to |module|'s directory. Returns a
// reference to the loaded module on success, or null on error.
HMODULE LoadModuleWithDirectory(const base::FilePath& module) {
  ::SetCurrentDirectoryW(module.DirName().value().c_str());
  base::PreReadFile(module, /*is_executable=*/true);
  return ::LoadLibraryExW(module.value().c_str(), nullptr,
                          LOAD_WITH_ALTERED_SEARCH_PATH);
}

void RecordDidRun(const base::FilePath& dll_path) {
  GoogleUpdateSettings::UpdateDidRunState(true);
}

bool ProcessTypeUsesMainDll(const std::string& process_type) {
  return process_type.empty() ||
         process_type == switches::kCloudPrintServiceProcess;
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
// directory) are suported. The identified file is not guaranteed to exist.
base::FilePath GetModulePath(base::StringPiece16 module_name) {
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

}  // namespace

//=============================================================================

MainDllLoader::MainDllLoader()
    : dll_(nullptr) {
}

MainDllLoader::~MainDllLoader() {
}

HMODULE MainDllLoader::Load(base::FilePath* module) {
  const base::char16* dll_name = nullptr;
  if (ProcessTypeUsesMainDll(process_type_)) {
    dll_name = installer::kChromeDll;
  } else if (process_type_ == switches::kWatcherProcess) {
    dll_name = kChromeWatcherDll;
  } else {
#if defined(CHROME_MULTIPLE_DLL)
    dll_name = installer::kChromeChildDll;
#else
    dll_name = installer::kChromeDll;
#endif
  }

  *module = GetModulePath(dll_name);
  if (module->empty()) {
    PLOG(ERROR) << "Cannot find module " << dll_name;
    return nullptr;
  }
  HMODULE dll = LoadModuleWithDirectory(*module);
  if (!dll) {
    PLOG(ERROR) << "Failed to load Chrome DLL from " << module->value();
    return nullptr;
  }

  DCHECK(dll);
  return dll;
}

// Launching is a matter of loading the right dll and calling the entry point.
// Derived classes can add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance,
                          base::TimeTicks exe_entry_point_ticks) {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  process_type_ = cmd_line.GetSwitchValueASCII(switches::kProcessType);

  base::FilePath file;

  if (process_type_ == switches::kWatcherProcess) {
    chrome::RegisterPathProvider();

    base::win::ScopedHandle parent_process;
    base::win::ScopedHandle on_initialized_event;
    DWORD main_thread_id = 0;
    if (!InterpretChromeWatcherCommandLine(cmd_line, &parent_process,
                                           &main_thread_id,
                                           &on_initialized_event)) {
      return chrome::RESULT_CODE_UNSUPPORTED_PARAM;
    }

    base::FilePath watcher_data_directory;
    if (!base::PathService::Get(chrome::DIR_WATCHER_DATA,
                                &watcher_data_directory))
      return chrome::RESULT_CODE_MISSING_DATA;

    // Intentionally leaked.
    HMODULE watcher_dll = Load(&file);
    if (!watcher_dll)
      return chrome::RESULT_CODE_MISSING_DATA;

    ChromeWatcherMainFunction watcher_main =
        reinterpret_cast<ChromeWatcherMainFunction>(
            ::GetProcAddress(watcher_dll, kChromeWatcherDLLEntrypoint));
    return watcher_main(chrome::GetBrowserExitCodesRegistryPath().c_str(),
                        parent_process.Take(), main_thread_id,
                        on_initialized_event.Take(),
                        watcher_data_directory.value().c_str());
  }

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  const bool is_browser = process_type_.empty();
  const bool is_sandboxed =
      !cmd_line.HasSwitch(service_manager::switches::kNoSandbox);
  if (is_browser || is_sandboxed) {
    // For child processes that are running as --no-sandbox, don't initialize
    // the sandbox info, otherwise they'll be treated as brokers (as if they
    // were the browser).
    content::InitializeSandboxInfo(&sandbox_info);
  }

  dll_ = Load(&file);
  if (!dll_)
    return chrome::RESULT_CODE_MISSING_DATA;

  OnBeforeLaunch(cmd_line, process_type_, file);
  DLL_MAIN chrome_main =
      reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll_, "ChromeMain"));
  int rc = chrome_main(instance, &sandbox_info,
                       exe_entry_point_ticks.ToInternalValue());
  OnBeforeExit(file);
  return rc;
}

void MainDllLoader::RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  if (!dll_)
    return;

  RelaunchChromeBrowserWithNewCommandLineIfNeededFunc relaunch_function =
      reinterpret_cast<RelaunchChromeBrowserWithNewCommandLineIfNeededFunc>(
          ::GetProcAddress(dll_,
                           "RelaunchChromeBrowserWithNewCommandLineIfNeeded"));
  if (relaunch_function) {
    relaunch_function();
  } else if (ProcessTypeUsesMainDll(process_type_)) {
    LOG(DFATAL) << "Could not find exported function "
                << "RelaunchChromeBrowserWithNewCommandLineIfNeeded "
                << "(" << process_type_ << " process)";
  }
}

//=============================================================================

class ChromeDllLoader : public MainDllLoader {
 protected:
  // MainDllLoader implementation.
  void OnBeforeLaunch(const base::CommandLine& cmd_line,
                      const std::string& process_type,
                      const base::FilePath& dll_path) override;
  void OnBeforeExit(const base::FilePath& dll_path) override;

 private:
  std::unique_ptr<ChromeWatcherClient> chrome_watcher_client_;
};

void ChromeDllLoader::OnBeforeLaunch(const base::CommandLine& cmd_line,
                                     const std::string& process_type,
                                     const base::FilePath& dll_path) {
  if (process_type.empty()) {
    if (ShouldRecordActiveUse(cmd_line))
      RecordDidRun(dll_path);

    // Launch the watcher process.
    base::FilePath exe_path;
    if (base::PathService::Get(base::FILE_EXE, &exe_path)) {
      chrome_watcher_client_.reset(new ChromeWatcherClient(
          base::Bind(&GenerateChromeWatcherCommandLine, exe_path)));
      chrome_watcher_client_->LaunchWatcher();
    }
  } else {
    // Set non-browser processes up to be killed by the system after the browser
    // goes away. The browser uses the default shutdown order, which is 0x280.
    // Note that lower numbers here denote "kill later" and higher numbers mean
    // "kill sooner".
    // This gets rid of most of those unsighly sad tabs on logout and shutdown.
    ::SetProcessShutdownParameters(0x280 - 1, SHUTDOWN_NORETRY);
  }
}

void ChromeDllLoader::OnBeforeExit(const base::FilePath& dll_path) {
  chrome_watcher_client_.reset();
}

//=============================================================================

class ChromiumDllLoader : public MainDllLoader {
 protected:
  void OnBeforeLaunch(const base::CommandLine& cmd_line,
                      const std::string& process_type,
                      const base::FilePath& dll_path) override {}
  void OnBeforeExit(const base::FilePath& dll_path) override {}
};

MainDllLoader* MakeMainDllLoader() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return new ChromeDllLoader();
#else
  return new ChromiumDllLoader();
#endif
}
