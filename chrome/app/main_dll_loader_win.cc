// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/main_dll_loader_win.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <userenv.h>

#include <memory>
#include <optional>
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
#include "base/hash/hash.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/self_deleting.h"
#include "base/path_service.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/shlwapi.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/app/llvm_profile_util.h"
#include "chrome/browser/active_use_util.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/update_did_run_state.h"
#include "chrome/installer/util/util_constants.h"
#include "components/activity_reporter/buildflags.h"
#include "components/startup_metric_utils/common/preread_experiment_group.h"
#include "components/version_info/channel.h"
#include "content/public/app/sandbox_helper_win.h"
#include "content/public/common/content_switches.h"
#include "sandbox/win/src/sandbox.h"

namespace {

using startup_metric_utils::PrereadExperimentGroup;

class DllPreReader : public base::PlatformThread::Delegate,
                     public base::SelfDeleting {
 public:
  explicit DllPreReader(const base::FilePath& module,
                        base::SelfDeletingPassKey key)
      : SelfDeleting(key), module_(module) {}

  DllPreReader(const DllPreReader&) = delete;
  DllPreReader& operator=(const DllPreReader&) = delete;

  void ThreadMain() override {
    base::PreReadFile(module_, /*is_executable=*/true, /*sequential=*/false);
    // As a non-joinable thread delegate, this class must clean itself up when
    // finished.
    delete this;
  }

 private:
  ~DllPreReader() override = default;

  base::FilePath module_;
};

// The entry point signature of all main modules.
typedef int (*DLL_MAIN)(HINSTANCE,
                        sandbox::SandboxInterfaceInfo*,
                        int64_t exe_main_entry_point_ticks,
                        int64_t preread_begin_ticks,
                        int64_t preread_end_ticks,
                        PrereadExperimentGroup preread_group);

// Properties for the main module to be loaded.
struct ModuleProperties {
  // The basename of the module (e.g., "chrome.dll").
  base::FilePath::StringViewType module_name;

  // The name of the main entrypoint of the module (e.g., "ChromeMain").
  base::cstring_view entrypoint_name;

  // The profile type to configure for PGO, if any.
  std::optional<ProfileProcessType> profile_type;
};

// Returns the path to the patched renderer DLL if a valid version string and
// user path component are specified on the command line, or an empty FilePath
// otherwise. Does not validate whether the binary exists or is readable on
// disk.
base::FilePath GetPatchedRendererPath() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return child_module::GetRendererBinaryPath(
      child_module::GetModulesDirForUser(command_line.GetSwitchValuePath(
          switches::kChildModuleUserPathComponent)),
      base::Version(
          command_line.GetSwitchValueASCII(switches::kChildModuleVersion)));
}

// Returns the properties for the module to be loaded in `process_type`.
const ModuleProperties& ModulePropertiesFromProcessType(
    std::string_view process_type) {
  // Most process types load chrome.dll and run `ChromeMain`.
  static constexpr ModuleProperties kOtherProperties = {
      installer::kChromeDll, "ChromeMain", std::nullopt};
  // Renderers load chrome_renderer.dll and run `ChromeRendererMain`.
  static constexpr ModuleProperties kRendererProperties = {
      chrome::kRendererDll, "ChromeRendererMain",
      ProfileProcessType::kRenderer};

#if BUILDFLAG(ENABLE_SEPARATE_RENDERER_BINARY)
  if (process_type == switches::kRendererProcess) {
    return kRendererProperties;
  }
#else
  if (process_type == switches::kRendererProcess &&
      !GetPatchedRendererPath().empty()) {
    return kRendererProperties;
  }
#endif

  return kOtherProperties;
}

std::wstring GetMachineGuid() {
  base::win::RegKey key;
  std::wstring value;
  if (key.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
               KEY_QUERY_VALUE | KEY_WOW64_64KEY) != ERROR_SUCCESS ||
      key.ReadValue(L"MachineGuid", &value) != ERROR_SUCCESS || value.empty()) {
    return std::wstring();
  }
  return value;
}

// Uses the machine GUID to determine whether preload should be run
// asynchronously as part of the ParallelPreReadFileMainDllWin synthetic trial.
// The GUID is used to be stable across sessions. This is important when
// affecting pre-reading because there is a learning effect for preloading
// interventions at the OS level.
PrereadExperimentGroup GetPreReadExperimentGroup() {
  // Get the machine GUID, in case that fails return kUnknown to default to
  // pre-reading outside trial.
  const std::wstring machine_guid = GetMachineGuid();
  if (machine_guid.empty()) {
    return PrereadExperimentGroup::kUnknown;
  }

  const uint32_t hash = base::PersistentHash(base::as_byte_span(machine_guid));
  // 1% experiment on stable.
  const version_info::Channel channel = install_static::GetChromeChannel();
  if (channel == version_info::Channel::STABLE) {
    if (hash % 100 == 0) {
      return PrereadExperimentGroup::kParallel;
    }
    if (hash % 100 == 1) {
      return PrereadExperimentGroup::kSynchronous;
    }
    return PrereadExperimentGroup::kUnknown;
  }

  // 50% Parallel, 50% Synchronous on lower channels.
  return (hash % 2 == 0) ? PrereadExperimentGroup::kParallel
                         : PrereadExperimentGroup::kSynchronous;
}

void RecordDidRun(const base::FilePath& dll_path) {
#if BUILDFLAG(USE_LEGACY_ACTIVE_DEFINITION)
  installer::UpdateDidRunState();
#endif
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
  if (module_name == chrome::kRendererDll) {
    base::FilePath patched_path = GetPatchedRendererPath();
    if (!patched_path.empty()) {
      return patched_path;
    }
  }

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

  // Otherwise, return the path to the module in the current executable's
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
                                base::TimeTicks& preread_end_ticks,
                                PrereadExperimentGroup& preread_group) {
  ::SetCurrentDirectoryW(module.DirName().value().c_str());
  // Perform group selection for the ParallelPreReadFileMainDllWin field trial
  // in the browser process. Other processes do not participate in the study.
  preread_group = is_browser ? GetPreReadExperimentGroup()
                             : PrereadExperimentGroup::kUnknown;

  if (is_browser && preread_group == PrereadExperimentGroup::kParallel) {
    // Preread the main DLL asynchronously in the browser process if this client
    // is assigned to the parallel group of the study.
    base::PlatformThread::CreateNonJoinableWithType(
        0, base::MakeSelfDeleting<DllPreReader>(module),
        base::ThreadType::kDefault);
  } else if (is_browser || !cmd_line.HasSwitch(switches::kNoPreReadMainDll)) {
    // Otherwise, preread synchronously for all other browser processes and for
    // child processes for which the PerformanceManager did not suppress
    // pre-read.
    preread_begin_ticks = base::TimeTicks::Now();
    base::PreReadFile(module, /*is_executable=*/true, /*sequential=*/false);
    preread_end_ticks = base::TimeTicks::Now();
  }

  HMODULE handle = ::LoadLibraryExW(module.value().c_str(), nullptr,
                                    LOAD_WITH_ALTERED_SEARCH_PATH);
  return handle;
}

// Prefetches and loads `module_name`. Populates `module` with the path of the
// loaded DLL. Returns a handle to the loaded DLL, or nullptr on failure.
HMODULE Load(base::FilePath* module,
             const base::FilePath::StringViewType module_name,
             const base::CommandLine& cmd_line,
             bool is_browser,
             base::TimeTicks& preread_begin_ticks,
             base::TimeTicks& preread_end_ticks,
             PrereadExperimentGroup& preread_group) {
  *module = GetModulePath(module_name);
  if (module->empty()) {
    PLOG(ERROR) << "Cannot find module " << module_name;
    return nullptr;
  }
  HMODULE dll = LoadModuleWithDirectory(*module, cmd_line, is_browser,
                                        preread_begin_ticks, preread_end_ticks,
                                        preread_group);
  if (!dll) {
    PLOG(ERROR) << "Failed to load Chrome DLL from " << module->value();
  }
  return dll;
}

}  // namespace

//=============================================================================

MainDllLoader::MainDllLoader() : dll_(nullptr) {}

MainDllLoader::~MainDllLoader() = default;

// Launching is a matter of loading the right dll and calling the entry point.
// Derived classes can add custom code in the OnBeforeLaunch callback.
int MainDllLoader::Launch(HINSTANCE instance,
                          base::TimeTicks exe_entry_point_ticks) {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  process_type_ = cmd_line.GetSwitchValueASCII(switches::kProcessType);

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  content::InitializeSandboxInfo(
      &sandbox_info, IsExtensionPointDisableSet()
                         ? sandbox::MITIGATION_EXTENSION_POINT_DISABLE
                         : 0);

  base::TimeTicks preread_begin_ticks;
  base::TimeTicks preread_end_ticks;
  PrereadExperimentGroup preread_group = PrereadExperimentGroup::kUnknown;

  // Determine the names of the module to load and its main entrypoint.
  const auto& module_properties =
      ModulePropertiesFromProcessType(process_type_);

  if (module_properties.profile_type.has_value()) {
    SetLLVMProfileProcessType(*module_properties.profile_type);
  }

  const bool is_browser = process_type_.empty();
  base::FilePath file;
  dll_ = Load(&file, module_properties.module_name, cmd_line, is_browser,
              preread_begin_ticks, preread_end_ticks, preread_group);
  if (!dll_)
    return CHROME_RESULT_CODE_MISSING_DATA;

  OnBeforeLaunch(process_type_, file);
  DLL_MAIN chrome_main = reinterpret_cast<DLL_MAIN>(
      ::GetProcAddress(dll_, module_properties.entrypoint_name.c_str()));
  int rc = chrome_main(instance, &sandbox_info,
                       exe_entry_point_ticks.ToInternalValue(),
                       preread_begin_ticks.ToInternalValue(),
                       preread_end_ticks.ToInternalValue(), preread_group);
  return rc;
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
