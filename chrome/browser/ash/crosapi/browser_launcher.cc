// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include <fcntl.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/barrier_closure.h"
#include "base/base_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/primary_profile_creation_waiter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/standalone_browser/channel_util.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/startup/startup_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/nacl/common/buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/values_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/device_ownership_waiter.h"
#include "components/user_manager/device_ownership_waiter_impl.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/resource/temporary_shared_resource_path_chromeos.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_switches.h"
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/media/cdm_registration.h"
#endif

namespace crosapi {

namespace {

using LaunchParamsFromBackground = BrowserLauncher::LaunchParamsFromBackground;
using LaunchParams = BrowserLauncher::LaunchParams;
using LaunchResults = BrowserLauncher::LaunchResults;

// Resources file sharing mode.
enum class ResourcesFileSharingMode {
  kDefault = 0,
  // Failed to handle cached shared resources properly.
  kError = 1,
};

// Global flag to skip the device ownership fetch. Global because some tests
// need to set this value before BrowserManager is constructed.
bool g_skip_device_ownership_wait_for_testing = false;

base::FilePath LacrosLogDirectory() {
  return browser_util::GetUserDataDir();
}

base::FilePath LacrosLogPath() {
  return LacrosLogDirectory().Append("lacros.log");
}

base::FilePath LacrosCrashDumpDirectory() {
  return LacrosLogDirectory().Append("Crash Reports");
}

std::string GetXdgRuntimeDir() {
  // If ash-chrome was given an environment variable, use it.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string xdg_runtime_dir;
  if (env->GetVar("XDG_RUNTIME_DIR", &xdg_runtime_dir)) {
    return xdg_runtime_dir;
  }

  // Otherwise provide the default for Chrome OS devices.
  return "/run/chrome";
}

// Rotate existing Lacros's log file. Returns true if a log file existed before
// being moved, and false if no log file was found.
bool RotateLacrosLogs() {
  base::FilePath log_path = LacrosLogPath();
  if (!base::PathExists(log_path)) {
    return false;
  }

  if (!logging::RotateLogFile(log_path)) {
    PLOG(ERROR) << "Failed to rotate the log file: " << log_path.value()
                << ". Keeping using the same log file without rotating.";
  }
  return true;
}

ResourcesFileSharingMode ClearOrMoveSharedResourceFileInternal(
    bool clear_shared_resource_file,
    base::FilePath shared_resource_path) {
  // If shared resource pak doesn't exit, do nothing.
  if (!base::PathExists(shared_resource_path)) {
    return ResourcesFileSharingMode::kDefault;
  }

  // Clear shared resource file cache if `clear_shared_resource_file` is true.
  if (clear_shared_resource_file) {
    if (!base::DeleteFile(shared_resource_path)) {
      LOG(ERROR) << "Failed to delete cached shared resource file.";
      return ResourcesFileSharingMode::kError;
    }
    return ResourcesFileSharingMode::kDefault;
  }

  base::FilePath renamed_shared_resource_path =
      ui::GetPathForTemporarySharedResourceFile(shared_resource_path);

  // Move shared resource pak to `renamed_shared_resource_path`.
  if (!base::Move(shared_resource_path, renamed_shared_resource_path)) {
    LOG(ERROR) << "Failed to move cached shared resource file to temporary "
               << "location.";
    return ResourcesFileSharingMode::kError;
  }
  return ResourcesFileSharingMode::kDefault;
}

ResourcesFileSharingMode ClearOrMoveSharedResourceFile(
    bool clear_shared_resource_file) {
  // Check 3 resource paks, resources.pak, chrome_100_percent.pak and
  // chrome_200_percent.pak.
  ResourcesFileSharingMode resources_file_sharing_mode =
      ResourcesFileSharingMode::kDefault;
  // Return kError if any of the resources failed to clear or move.
  // Make sure that ClearOrMoveSharedResourceFileInternal() runs for all
  // resources even if it already fails for some resource.
  if (ClearOrMoveSharedResourceFileInternal(
          clear_shared_resource_file, browser_util::GetUserDataDir().Append(
                                          crosapi::kSharedResourcesPackName)) ==
      ResourcesFileSharingMode::kError) {
    resources_file_sharing_mode = ResourcesFileSharingMode::kError;
  }
  if (ClearOrMoveSharedResourceFileInternal(
          clear_shared_resource_file,
          browser_util::GetUserDataDir().Append(
              crosapi::kSharedChrome100PercentPackName)) ==
      ResourcesFileSharingMode::kError) {
    resources_file_sharing_mode = ResourcesFileSharingMode::kError;
  }
  if (ClearOrMoveSharedResourceFileInternal(
          clear_shared_resource_file,
          browser_util::GetUserDataDir().Append(
              crosapi::kSharedChrome200PercentPackName)) ==
      ResourcesFileSharingMode::kError) {
    resources_file_sharing_mode = ResourcesFileSharingMode::kError;
  }
  return resources_file_sharing_mode;
}

// This method runs some work on a background thread prior to launching lacros.
// The returns struct is used by the main thread as parameters to launch Lacros.
void DoLacrosBackgroundWorkPreLaunch(
    const base::FilePath& lacros_dir,
    bool clear_shared_resource_file,
    BrowserLauncher::LaunchParamsFromBackground& params) {
  if (!RotateLacrosLogs()) {
    // If log file does not exist, most likely the user directory does not
    // exist either. So create it here.
    base::File::Error error;
    base::FilePath lacros_log_dir = LacrosLogDirectory();
    if (!base::CreateDirectoryAndGetError(lacros_log_dir, &error)) {
      LOG(ERROR) << "Failed to make directory " << lacros_log_dir << ": "
                 << base::File::ErrorToString(error);
      return;
    }
  }

  int fd = HANDLE_EINTR(
      open(LacrosLogPath().value().c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644));

  if (fd < 0) {
    PLOG(ERROR) << "Failed to get file descriptor for " << LacrosLogPath();
    return;
  }

  params.logfd = base::ScopedFD(fd);

  params.enable_resource_file_sharing =
      base::FeatureList::IsEnabled(features::kLacrosResourcesFileSharing);
  // If resource file sharing feature is disabled, clear the cached shared
  // resource file anyway.
  if (!params.enable_resource_file_sharing) {
    clear_shared_resource_file = true;
  }

  // Clear shared resource file cache if it's initial lacros launch after ash
  // reboot. If not, rename shared resource file cache to temporal name on
  // Lacros launch.
  if (ClearOrMoveSharedResourceFile(clear_shared_resource_file) ==
      ResourcesFileSharingMode::kError) {
    params.enable_resource_file_sharing = false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kLacrosChromeAdditionalArgsFile)) {
    const base::FilePath path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            ash::switches::kLacrosChromeAdditionalArgsFile);
    std::string data;
    if (!base::ReadFileToString(path, &data)) {
      PLOG(WARNING) << "Unable to read from lacros additional args file "
                    << path.value();
    }
    std::vector<std::string_view> delimited_flags =
        base::SplitStringPieceUsingSubstr(data, "\n", base::TRIM_WHITESPACE,
                                          base::SPLIT_WANT_NONEMPTY);

    for (const auto& flag : delimited_flags) {
      if (flag[0] != '#') {
        params.lacros_additional_args.emplace_back(flag);
      }
    }
  }
}

void TerminateProcessBackground(base::Process process,
                                base::TimeDelta timeout) {
  // Here, lacros-chrome process may crash, or be in the shutdown procedure.
  // Give some amount of time for the collection. In most cases,
  // this waits until it captures the process termination.
  if (process.WaitForExitWithTimeout(timeout, nullptr)) {
    return;
  }

  // Here, the process is not yet terminated.
  // This happens if some critical error happens on the mojo connection,
  // while both ash-chrome and lacros-chrome are still alive.
  // Terminate the lacros-chrome.
  bool success = process.Terminate(/*exit_code=*/0, /*wait=*/true);
  LOG_IF(ERROR, !success) << "Failed to terminate the lacros-chrome.";
}

// NOTE: Do NOT add the command line here unless it is very fundamental. Find
// the method suited the best from `SetUp*` or create a new one.
base::CommandLine CreateCommandLine(const base::FilePath& chrome_path) {
  base::CommandLine command_line(chrome_path);

  command_line.AppendSwitchASCII(switches::kOzonePlatform, "wayland");

  // Paths are UTF-8 safe on Chrome OS.
  command_line.AppendSwitchASCII("user-data-dir",
                                 browser_util::GetUserDataDir().AsUTF8Unsafe());

  // Passes the locale via command line instead of via LacrosInitParams because
  // the Lacros browser process needs it early in startup, before zygote fork.
  command_line.AppendSwitchASCII(switches::kLang,
                                 g_browser_process->GetApplicationLocale());

#if BUILDFLAG(USE_CRAS)
  // CrAS is the default audio server in Chrome OS.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    command_line.AppendSwitch(switches::kUseCras);
  }
#endif
  return command_line;
}

// NOTE: Do NOT add the options here unless it is very fundamental. Find
// the method suited the best from `SetUp*` or create a new one.
base::LaunchOptions CreateLaunchOptions() {
  base::LaunchOptions options;
  options.kill_on_parent_death = true;
  return options;
}

void SetUpEnvironment(ash::standalone_browser::LacrosSelection lacros_selection,
                      base::LaunchOptions& options) {
  // If Ash is an unknown channel then this is not a production build and we
  // should be using an unknown channel for Lacros as well. This prevents Lacros
  // from picking up Finch experiments.
  version_info::Channel update_channel = version_info::Channel::UNKNOWN;
  if (ash::GetChannel() != version_info::Channel::UNKNOWN) {
    update_channel = ash::standalone_browser::GetLacrosSelectionUpdateChannel(
        lacros_selection);
    // If we don't have channel information, we default to the "dev" channel.
    if (update_channel == version_info::Channel::UNKNOWN) {
      update_channel = ash::standalone_browser::kLacrosDefaultChannel;
    }
  }

  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = GetXdgRuntimeDir();
  options.environment["CHROME_VERSION_EXTRA"] =
      version_info::GetChannelString(update_channel);

  if (base::FeatureList::IsEnabled(ash::features::kLacrosWaylandLogging)) {
    options.environment["WAYLAND_DEBUG"] = "1";
  }

  // LsbRelease and LsbReleaseTime are used by sys_info in Lacros to determine
  // hardware class.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string lsb_release;
  std::string lsb_release_time;
  if (env->GetVar(base::kLsbReleaseKey, &lsb_release) &&
      env->GetVar(base::kLsbReleaseTimeKey, &lsb_release_time)) {
    options.environment[base::kLsbReleaseKey] = std::move(lsb_release);
    options.environment[base::kLsbReleaseTimeKey] = std::move(lsb_release_time);
  }
}

void SetUpForDevMode(base::CommandLine& command_line) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSystemDevMode)) {
    command_line.AppendSwitch(chromeos::switches::kSystemDevMode);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowRAInDevMode)) {
    command_line.AppendSwitch(switches::kAllowRAInDevMode);
  }
}

#if BUILDFLAG(ENABLE_NACL)
void SetUpForNacl(base::CommandLine& command_line) {
  // This switch is forwarded to nacl_helper and is needed before zygote fork.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVerboseLoggingInNacl)) {
    command_line.AppendSwitchASCII(
        switches::kVerboseLoggingInNacl,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kVerboseLoggingInNacl));
  }
}
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
void SetUpForWidevine(base::CommandLine& command_line) {
  if (base::FeatureList::IsEnabled(media::kLacrosUseAshWidevine)) {
    // These directories are needed to load the Widevine CDM before zygote fork.
    base::FilePath bundled_dir;
    if (base::PathService::Get(chrome::DIR_BUNDLED_WIDEVINE_CDM,
                               &bundled_dir)) {
      command_line.AppendSwitchASCII(switches::kCrosWidevineBundledDir,
                                     bundled_dir.AsUTF8Unsafe());
    }

    base::FilePath component_updated_hint_file;
    if (base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                               &component_updated_hint_file)) {
      command_line.AppendSwitchASCII(
          switches::kCrosWidevineComponentUpdatedHintFile,
          component_updated_hint_file.AsUTF8Unsafe());
    }
  }
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

void SetUpLacrosAdditionalParameters(const LaunchParamsFromBackground& params,
                                     LaunchParams& parameters) {
  std::string additional_env =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kLacrosChromeAdditionalEnv);
  base::StringPairs env_pairs;
  if (base::SplitStringIntoKeyValuePairsUsingSubstr(additional_env, '=', "####",
                                                    &env_pairs)) {
    for (const auto& env_pair : env_pairs) {
      if (!env_pair.first.empty()) {
        LOG(WARNING) << "Applying lacros env " << env_pair.first << "="
                     << env_pair.second;
        parameters.options.environment[env_pair.first] = env_pair.second;
      }
    }
  }

  std::string additional_flags =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kLacrosChromeAdditionalArgs);

  // `addtional_flags` is a string composed with flags and "####" is in between
  // flags and this has to be separated one by one.
  // TODO(elkurin): We should console an error log if flags are not in the
  // correct format. For example, If "###" is in between flags, they become 1
  // flag without an error for now.
  std::vector<std::string> delimited_flags = base::SplitStringUsingSubstr(
      additional_flags, "####", base::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);

  parameters.command_line.AppendArguments(
      base::CommandLine::FromArgvWithoutProgram(delimited_flags), false);
  parameters.command_line.AppendArguments(
      base::CommandLine::FromArgvWithoutProgram(params.lacros_additional_args),
      false);
}

void SetUpForGpu(base::CommandLine& command_line) {
  command_line.AppendSwitch(switches::kEnableGpuRasterization);
  command_line.AppendSwitch(switches::kEnableWebGLImageChromium);
  // Forward flag for zero copy video capture to Lacros if it is enabled.
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    command_line.AppendSwitch(switches::kVideoCaptureUseGpuMemoryBuffer);
  }
}

void SetUpLogging(std::optional<int> logfd, LaunchParams& parameters) {
  // If logfd is valid, enable logging and redirect stdout/stderr to logfd.
  if (!logfd) {
    return;
  }
  // The next flag will make chrome log only via stderr. See
  // DetermineLoggingDestination in logging_chrome.cc.
  parameters.command_line.AppendSwitchASCII(switches::kEnableLogging, "stderr");

  auto* current_command_line = base::CommandLine::ForCurrentProcess();
  if (current_command_line->HasSwitch(switches::kLoggingLevel)) {
    parameters.command_line.AppendSwitchASCII(
        switches::kLoggingLevel,
        current_command_line->GetSwitchValueASCII(switches::kLoggingLevel));
  }

  parameters.command_line.AppendSwitchASCII(
      switches::kVModule,
      // TODO(crbug.com/40061238): Remove after fix.
      "wayland_window_drag_controller=1,wayland_data_source=1,tab_drag_"
      // TODO(crbug.com/40069512): Remove after fix.
      "controller=1, wayland_data_drag_controller=1");

  // These options will assign stdout/stderr fds to logfd in the fd table of
  // the new process.
  parameters.options.fds_to_remap.push_back(
      std::make_pair(logfd.value(), STDOUT_FILENO));
  parameters.options.fds_to_remap.push_back(
      std::make_pair(logfd.value(), STDERR_FILENO));
}

// Sets up switches and arguments of command line for startup data.
void SetUpForStartupData(std::optional<int> startup_fd,
                         LaunchParams& parameters) {
  if (startup_fd) {
    // Hardcoded to use FD 3 to make the ash-chrome's behavior more predictable.
    // Lacros-chrome should not depend on the hardcoded value though. Instead
    // it should take a look at the value passed via the command line flag.
    constexpr int kStartupDataFD = 3;
    parameters.command_line.AppendSwitchASCII(
        chromeos::switches::kCrosStartupDataFD,
        base::NumberToString(kStartupDataFD));
    parameters.options.fds_to_remap.emplace_back(startup_fd.value(),
                                                 kStartupDataFD);
  }
}

void SetUpForMojo(mojo::PlatformChannel& channel, LaunchParams& parameters) {
  // Prepare to invite lacros-chrome to the Mojo universe of Crosapi.
  std::string channel_flag_value;
  channel.PrepareToPassRemoteEndpoint(&parameters.options.fds_to_remap,
                                      &channel_flag_value);
  CHECK(!channel_flag_value.empty());
  parameters.command_line.AppendSwitchASCII(kCrosapiMojoPlatformChannelHandle,
                                            channel_flag_value);
}

void SetUpForCrashpad(base::CommandLine& command_line) {
  // Paths are UTF-8 safe on Chrome OS.
  std::string crash_dir = LacrosCrashDumpDirectory().AsUTF8Unsafe();
  command_line.AppendSwitchASCII("breakpad-dump-location", crash_dir);
}

// Sets up switches and arguments of command line for anything shared to
// Lacros.
void SetUpFeatures(const LaunchParamsFromBackground& params,
                   LaunchParams& parameters) {
  if (params.enable_resource_file_sharing) {
    // Passes a flag to enable resources file sharing to Lacros.
    // To use resources file sharing feature on Lacros, it's required for ash to
    // run with enabling the feature as well since the feature is based on some
    // ash behavior(clear or move cached shared resource file at lacros launch).
    parameters.command_line.AppendSwitch(switches::kEnableResourcesFileSharing);
  }
}

}  // namespace

// To be sure the lacros is running with neutral thread type.
class LacrosThreadTypeDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  void RunAsyncSafe() override {
    // TODO(crbug.com/40212082): Currently, this is causing some deadlock issue.
    // It looks like inside the function, we seem to call async unsafe API.
    // For the mitigation, disabling this temporarily.
    // We should revisit here, and see the impact of performance.
    // SetCurrentThreadType() needs file I/O on /proc and /sys.
    // base::ScopedAllowBlocking allow_blocking;
    // base::PlatformThread::SetCurrentThreadType(
    //     base::ThreadType::kDefault);
  }
};

BrowserLauncher::BrowserLauncher()
    : device_ownership_waiter_(
          std::make_unique<user_manager::DeviceOwnershipWaiterImpl>()) {}

BrowserLauncher::~BrowserLauncher() = default;

LaunchParamsFromBackground::LaunchParamsFromBackground() = default;
LaunchParamsFromBackground::LaunchParamsFromBackground(
    LaunchParamsFromBackground&&) = default;
LaunchParamsFromBackground& LaunchParamsFromBackground::operator=(
    LaunchParamsFromBackground&&) = default;
LaunchParamsFromBackground::~LaunchParamsFromBackground() = default;

LaunchParams::LaunchParams(base::CommandLine command_line,
                           base::LaunchOptions options)
    : command_line(std::move(command_line)), options(std::move(options)) {}
LaunchParams::LaunchParams(LaunchParams&&) = default;
LaunchParams& LaunchParams::operator=(LaunchParams&&) = default;
LaunchParams::~LaunchParams() = default;

LaunchResults::LaunchResults() = default;
LaunchResults::LaunchResults(LaunchResults&&) = default;
LaunchResults& LaunchResults::operator=(LaunchResults&&) = default;
LaunchResults::~LaunchResults() = default;

void BrowserLauncher::Launch(
    const base::FilePath& chrome_path,
    ash::standalone_browser::LacrosSelection lacros_selection,
    base::OnceClosure mojo_disconnection_cb,
    bool is_keep_alive_enabled,
    LaunchCompletionCallback callback) {
  auto* params = new LaunchParamsFromBackground();

  const int kNumTasks = 3;
  auto barrier_closure = base::BarrierClosure(
      kNumTasks, base::BindOnce(&BrowserLauncher::LaunchProcess,
                                weak_factory_.GetWeakPtr(), chrome_path,
                                base::WrapUnique(params), lacros_selection,
                                std::move(mojo_disconnection_cb),
                                is_keep_alive_enabled, std::move(callback)));

  // Prepare on the background thread.
  WaitForBackgroundWorkPreLaunch(chrome_path.DirName(), is_first_lacros_launch_,
                                 barrier_closure, *params);

  // Set false to prepare for the next Lacros launch.
  is_first_lacros_launch_ = false;

  WaitForDeviceOwnerFetchedAndThen(barrier_closure);
  WaitForPrimaryProfileAddedAndThen(barrier_closure);
}

bool BrowserLauncher::IsProcessValid() const {
  return process_.IsValid();
}

bool BrowserLauncher::TriggerTerminate(int exit_code) const {
  if (!process_.IsValid()) {
    return false;
  }

  process_.Terminate(/*exit_code=*/exit_code, /*wait=*/false);

  // TODO(mayukoaiba): We should reset `process_` by base::Process() in order to
  // manage the state of process properly
  return true;
}

void BrowserLauncher::EnsureProcessTerminated(base::OnceClosure callback,
                                              base::TimeDelta timeout) {
  CHECK(process_.IsValid());
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&TerminateProcessBackground, std::move(process_), timeout),
      std::move(callback));
  CHECK(!process_.IsValid());
}

const base::Process& BrowserLauncher::GetProcessForTesting() const {
  return process_;
}

bool BrowserLauncher::LaunchProcessForTesting(const LaunchParams& parameters) {
  return LaunchProcessWithParameters(parameters);
}

LaunchParams BrowserLauncher::CreateLaunchParamsForTesting(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    std::optional<int> startup_fd,
    mojo::PlatformChannel& channel,
    ash::standalone_browser::LacrosSelection lacros_selection) {
  return CreateLaunchParams(chrome_path, params, startup_fd, channel,
                            lacros_selection);
}

void BrowserLauncher::SetUpAdditionalParametersForTesting(
    LaunchParamsFromBackground& params,
    LaunchParams& parameters) const {
  SetUpLacrosAdditionalParameters(params, parameters);
}

void BrowserLauncher::WaitForBackgroundWorkPreLaunchForTesting(
    const base::FilePath& lacros_dir,
    bool clear_shared_resource_file,
    base::OnceClosure callback,
    LaunchParamsFromBackground& params) {
  WaitForBackgroundWorkPreLaunch(lacros_dir, clear_shared_resource_file,
                                 std::move(callback), params);
}

void BrowserLauncher::set_device_ownership_waiter_for_testing(
    std::unique_ptr<user_manager::DeviceOwnershipWaiter>
        device_ownership_waiter) {
  CHECK(!device_ownership_waiter_called_);
  device_ownership_waiter_ = std::move(device_ownership_waiter);
}

// static
void BrowserLauncher::SkipDeviceOwnershipWaitForTesting(bool skip) {
  CHECK_IS_TEST();
  g_skip_device_ownership_wait_for_testing = skip;
}

void BrowserLauncher::WaitForBackgroundWorkPreLaunch(
    const base::FilePath& lacros_dir,
    bool clear_shared_resource_file,
    base::OnceClosure callback,
    LaunchParamsFromBackground& params) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&DoLacrosBackgroundWorkPreLaunch, lacros_dir,
                     clear_shared_resource_file, std::ref(params)),
      base::BindOnce(std::move(callback)));
}

void BrowserLauncher::WaitForDeviceOwnerFetchedAndThen(
    base::OnceClosure callback) {
  if (g_skip_device_ownership_wait_for_testing) {
    CHECK_IS_TEST();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  } else {
    device_ownership_waiter_called_ = true;
    device_ownership_waiter_->WaitForOwnershipFetched(std::move(callback));
  }
}

void BrowserLauncher::WaitForPrimaryProfileAddedAndThen(
    base::OnceClosure callback) {
  CHECK(!primary_profile_creation_waiter_);
  primary_profile_creation_waiter_ = PrimaryProfileCreationWaiter::WaitOrRun(
      g_browser_process->profile_manager(), std::move(callback));
}

void BrowserLauncher::LaunchProcess(
    const base::FilePath& chrome_path,
    std::unique_ptr<LaunchParamsFromBackground> params,
    ash::standalone_browser::LacrosSelection lacros_selection,
    base::OnceClosure mojo_disconnection_cb,
    bool is_keep_alive_enabled,
    LaunchCompletionCallback callback) {
  if (shutdown_requested_) {
    std::move(callback).Run(
        base::unexpected(LaunchFailureReason::kShutdownRequested));
    return;
  }

  LOG(WARNING) << "Starting lacros-chrome launching at "
               << chrome_path.MaybeAsASCII();

  // Creates FD for startup.
  base::ScopedFD startup_fd = browser_util::CreateStartupData(
      browser_util::InitialBrowserAction(
          mojom::InitialBrowserAction::kDoNotOpenWindow),
      !is_keep_alive_enabled, lacros_selection);

  // Sets up Mojo channel.
  // Uses new Crosapi mojo connection to detect process termination always.
  LaunchResults launch_results;
  mojo::PlatformChannel channel;
  launch_results.crosapi_id = CrosapiManager::Get()->SendInvitation(
      channel.TakeLocalEndpoint(), std::move(mojo_disconnection_cb));

  // Initialize command line and options for launching Lacros.
  // Do NOT include any codes with side effects because we just set up command
  // line and options in this function. Do NOT modify LaunchParams outside of
  // `CreateLaunchParams`.
  LaunchParams parameters = CreateLaunchParams(
      chrome_path, *params.get(),
      startup_fd.is_valid() ? std::optional(startup_fd.get()) : std::nullopt,
      channel, lacros_selection);

  base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
  launch_results.lacros_launch_time = base::TimeTicks::Now();

  bool success = LaunchProcessWithParameters(parameters);
  channel.RemoteProcessLaunchAttempted();

  // If Lacros failed to launch, it's most likely a permanent problem.
  if (!success) {
    std::move(callback).Run(base::unexpected(LaunchFailureReason::kUnknown));
    return;
  }

  std::move(callback).Run(base::ok(std::move(launch_results)));
}

LaunchParams BrowserLauncher::CreateLaunchParams(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    std::optional<int> startup_fd,
    mojo::PlatformChannel& channel,
    ash::standalone_browser::LacrosSelection lacros_selection) {
  // Static configuration should be enabled from Lacros rather than Ash. This
  // vector should only be used for dynamic configuration.
  // TODO(crbug.com/40729628): Remove existing static configuration.
  LaunchParams parameters(CreateCommandLine(chrome_path),
                          CreateLaunchOptions());

  SetUpEnvironment(lacros_selection, parameters.options);
  SetUpForDevMode(parameters.command_line);
#if BUILDFLAG(ENABLE_NACL)
  SetUpForNacl(parameters.command_line);
#endif
#if BUILDFLAG(ENABLE_WIDEVINE)
  SetUpForWidevine(parameters.command_line);
#endif
  SetUpForGpu(parameters.command_line);
  SetUpLogging(params.logfd.is_valid() ? std::optional(params.logfd.get())
                                       : std::nullopt,
               parameters);
  SetUpForStartupData(startup_fd, parameters);
  SetUpForMojo(channel, parameters);
  SetUpForCrashpad(parameters.command_line);

  // Ensures that child processes have the same rules about what help features,
  // sharing feature and location share may show as the current process.
  // NOTE: this may add an --enable-features flag to the command line if not
  // already present, or append to the flag if it is.
  feature_engagement::Tracker::PropagateTestStateToChildProcess(
      parameters.command_line);

  SetUpFeatures(params, parameters);

  // Process additional parameters at the end so that /etc/chrome_dev.conf can
  // override choices made by the SetUp* functions above.
  SetUpLacrosAdditionalParameters(params, parameters);

  return parameters;
}

bool BrowserLauncher::LaunchProcessWithParameters(
    const LaunchParams& parameters) {
  LOG(WARNING) << "Launching lacros with command: "
               << parameters.command_line.GetCommandLineString();

  // Create the lacros-chrome subprocess.
  // Checks whether process_ is valid or not in order not to overwrite
  // process_.
  CHECK(!process_.IsValid());
  // If process_ already exists, because it does not call waitpid(2),
  // the process will never be collected.
  process_ = base::LaunchProcess(parameters.command_line, parameters.options);

  if (!process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    return false;
  }
  LOG(WARNING) << "Launched lacros-chrome with pid " << process_.Pid();

  return true;
}

}  // namespace crosapi
