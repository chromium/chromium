// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/startup/startup_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/nacl/common/buildflags.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if defined(USE_CRAS)
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_switches.h"
#endif

namespace crosapi {

namespace {

using LaunchParamsFromBackground = BrowserLauncher::LaunchParamsFromBackground;

base::FilePath LacrosPostLoginLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log");
}

base::FilePath LacrosCrashDumpDirectory() {
  return BrowserLauncher::LacrosLogDirectory().Append("Crash Reports");
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

void AppendArguments(const std::vector<std::string>& args,
                     base::CommandLine& command_line) {
  // TODO(crbug.com/1513045): When std::vector<std::string> becomes directly
  // added to `base::CommandeLine` object, this function will be removed.
  // `AppendArg()` only calls` AppendArgNative()`, and does not separate the
  // flag into switches and keys. So, we need to separate the flag into switches
  // and keys before calling `AppendArg()` when each flag is added to
  // `command_line` with `AppendArg()` in for loop. In the current code,
  // `AppendArguments()` is responsible for creating the correct command line,
  // so even if something odd is added, it is likely to be detected here.
  base::CommandLine command_line_to_append =
      base::CommandLine(base::CommandLine::NO_PROGRAM);
  for (const auto& arg : args) {
    command_line_to_append.AppendArg(arg);
  }
  command_line.AppendArguments(command_line_to_append,
                               /*include_program=*/false);
}

// NOTE: Do NOT add the command line here unless it is very fundamental. Find
// the method suited the best from `SetUp*` or create a new one.
base::CommandLine CreateCommandLine(const base::FilePath& chrome_path) {
  base::CommandLine command_line = base::CommandLine(chrome_path);

  command_line.AppendSwitchASCII(switches::kOzonePlatform, "wayland");

  // Paths are UTF-8 safe on Chrome OS.
  command_line.AppendSwitchASCII("user-data-dir",
                                 browser_util::GetUserDataDir().AsUTF8Unsafe());

  // Passes the locale via command line instead of via LacrosInitParams because
  // the Lacros browser process needs it early in startup, before zygote fork.
  command_line.AppendSwitchASCII(switches::kLang,
                                 g_browser_process->GetApplicationLocale());

#if defined(USE_CRAS)
  // CrAS is the default audio server in Chrome OS.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    command_line.AppendSwitch(switches::kUseCras);
  }
#endif
  return command_line;
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

void SetUpLacrosAdditionalParameters(const LaunchParamsFromBackground& params,
                                     base::CommandLine& command_line) {
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

  // TODO(crbug.com/1513045): When `AppendSwitchesAndArguments` function in
  // base::CommandLine become public function, these vectors will be appended
  // `command_line` directly.
  AppendArguments(delimited_flags, command_line);
  AppendArguments(params.lacros_additional_args, command_line);
}

void SetUpForGpu(base::CommandLine& command_line) {
  command_line.AppendSwitch(switches::kEnableGpuRasterization);
  command_line.AppendSwitch(switches::kEnableWebGLImageChromium);
  // Forward flag for zero copy video capture to Lacros if it is enabled.
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    command_line.AppendSwitch(switches::kVideoCaptureUseGpuMemoryBuffer);
  }
}

// NOTE: Before calling this method, be sure to check `params.logfd` is
// valid.
void SetUpLogging(bool launching_at_login_screen,
                  base::CommandLine& command_line) {
  // The next flag will make chrome log only via stderr. See
  // DetermineLoggingDestination in logging_chrome.cc.
  command_line.AppendSwitchASCII(switches::kEnableLogging, "stderr");

  auto* current_command_line = base::CommandLine::ForCurrentProcess();
  if (current_command_line->HasSwitch(switches::kLoggingLevel)) {
    command_line.AppendSwitchASCII(
        switches::kLoggingLevel,
        current_command_line->GetSwitchValueASCII(switches::kLoggingLevel));
  }

  command_line.AppendSwitchASCII(
      switches::kVModule,
      // TODO(crbug.com/1371493): Remove after fix.
      "wayland_window_drag_controller=1,wayland_data_source=1,tab_drag_"
      // TODO(crbug.com/1472682): Remove after fix.
      "controller=1, wayland_data_drag_controller=1");

  if (launching_at_login_screen &&
      !current_command_line->HasSwitch(switches::kDisableLoggingRedirect)) {
    // Redirects logs to cryptohome after login on non-test images.
    command_line.AppendSwitchASCII(chromeos::switches::kCrosPostLoginLogFile,
                                   LacrosPostLoginLogPath().value());
  }
}

// Sets up switches and arguments of command line for startup and post-login
// data.
void SetUpForStartupData(std::optional<int> startup_data_fd,
                         std::optional<int> postlogin_data_fd,
                         base::CommandLine& command_line) {
  // TODO(mayukoaiba): This part and the one in BrowserManager depending on
  // whether `startup_fd` is valid or not need to be consistent.
  if (startup_data_fd) {
    command_line.AppendSwitchASCII(
        chromeos::switches::kCrosStartupDataFD,
        base::NumberToString(startup_data_fd.value()));
  }

  // TODO(mayukoaiba): This part and the one in BrowserManager depending on
  // `launching_at_login_screen` need to be consistent.
  if (postlogin_data_fd) {
    command_line.AppendSwitchASCII(
        chromeos::switches::kCrosPostLoginDataFD,
        base::NumberToString(postlogin_data_fd.value()));
  }
}

void SetUpForMojo(std::string_view channel_flag_value,
                  base::CommandLine& command_line) {
  CHECK(!channel_flag_value.empty());
  command_line.AppendSwitchASCII(kCrosapiMojoPlatformChannelHandle,
                                 channel_flag_value);
}

void SetUpForCrashpad(base::CommandLine& command_line) {
  // Paths are UTF-8 safe on Chrome OS.
  std::string crash_dir = LacrosCrashDumpDirectory().AsUTF8Unsafe();
  command_line.AppendSwitchASCII("breakpad-dump-location", crash_dir);

  if (crash_reporter::IsCrashpadEnabled()) {
    command_line.AppendSwitch(switches::kEnableCrashpad);
  }
}

// Sets up switches and arguments of command line for anything shared to
// Lacros.
void SetUpFeatures(const LaunchParamsFromBackground& params,
                   base::CommandLine& command_line) {
  if (params.enable_resource_file_sharing) {
    // Passes a flag to enable resources file sharing to Lacros.
    // To use resources file sharing feature on Lacros, it's required for ash to
    // run with enabling the feature as well since the feature is based on some
    // ash behavior(clear or move cached shared resource file at lacros launch).
    command_line.AppendSwitch(switches::kEnableResourcesFileSharing);
  }

  if (params.enable_shared_components_dir) {
    // Passes a flag to enable using a location shared across users for browser
    // components.
    command_line.AppendSwitch(switches::kEnableLacrosSharedComponentsDir);
  }
}

}  // namespace

BrowserLauncher::BrowserLauncher() = default;

BrowserLauncher::~BrowserLauncher() = default;

// static
base::FilePath BrowserLauncher::LacrosLogDirectory() {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  // When pre-launching Lacros at login screen is enabled:
  // - In test images, we always save Lacros logs in /var/log/lacros.
  // - In non-test images, we save Lacros logs in /var/log/lacros
  //   only when Lacros is running at login screen. Lacros will
  //   redirect user-specific logs to the cryptohome after login.
  // - In gLinux, there's no /var/log/lacros, so we stick with the
  //   default path.
  if (base::FeatureList::IsEnabled(browser_util::kLacrosLaunchAtLoginScreen) &&
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kDisableLoggingRedirect) ||
       session_manager::SessionManager::Get()->session_state() ==
           session_manager::SessionState::LOGIN_PRIMARY)) {
    return base::FilePath("/var/log/lacros");
  }
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)
  return browser_util::GetUserDataDir();
}

LaunchParamsFromBackground::LaunchParamsFromBackground() = default;
LaunchParamsFromBackground::LaunchParamsFromBackground(
    LaunchParamsFromBackground&&) = default;
LaunchParamsFromBackground& LaunchParamsFromBackground::operator=(
    LaunchParamsFromBackground&&) = default;
LaunchParamsFromBackground::~LaunchParamsFromBackground() = default;

bool BrowserLauncher::LaunchProcess(const base::FilePath& chrome_path,
                                    const LaunchParamsFromBackground& params,
                                    bool launching_at_login_screen,
                                    std::optional<int> startup_data_fd,
                                    std::optional<int> postlogin_data_fd,
                                    std::string_view channel_flag_value,
                                    const base::LaunchOptions& options) {
  base::CommandLine command_line(InitializeParameters(
      chrome_path, params, launching_at_login_screen, startup_data_fd,
      postlogin_data_fd, channel_flag_value));
  return LaunchProcessWithParameters(command_line, options);
}

bool BrowserLauncher::IsProcessValid() {
  return process_.IsValid();
}

bool BrowserLauncher::TriggerTerminate(int exit_code) {
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

base::CommandLine BrowserLauncher::InitializeParametersForTesting(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    bool launching_at_login_screen,
    std::optional<int> startup_data_fd,
    std::optional<int> postlogin_data_fd,
    std::string_view channel_flag_value) {
  return InitializeParameters(chrome_path, params, launching_at_login_screen,
                              startup_data_fd, postlogin_data_fd,
                              channel_flag_value);
}

const base::Process& BrowserLauncher::GetProcessForTesting() {
  return process_;
}

bool BrowserLauncher::LaunchProcessForTesting(
    const base::CommandLine& command_line,
    const base::LaunchOptions& options) {
  return LaunchProcessWithParameters(command_line, options);
}

void BrowserLauncher::SetUpAdditionalParametersForTesting(
    LaunchParamsFromBackground& params,
    base::CommandLine& command_line) {
  SetUpLacrosAdditionalParameters(params, command_line);
}

base::CommandLine BrowserLauncher::InitializeParameters(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    bool launching_at_login_screen,
    std::optional<int> startup_data_fd,
    std::optional<int> postlogin_data_fd,
    std::string_view channel_flag_value) {
  // Static configuration should be enabled from Lacros rather than Ash. This
  // vector should only be used for dynamic configuration.
  // TODO(https://crbug.com/1145713): Remove existing static configuration.
  base::CommandLine command_line(CreateCommandLine(chrome_path));

  SetUpForDevMode(command_line);
#if BUILDFLAG(ENABLE_NACL)
  SetUpForNacl(command_line);
#endif
  SetUpLacrosAdditionalParameters(params, command_line);
  SetUpForGpu(command_line);
  if (params.logfd.is_valid()) {
    SetUpLogging(launching_at_login_screen, command_line);
  }
  SetUpForStartupData(startup_data_fd, postlogin_data_fd, command_line);
  SetUpForMojo(channel_flag_value, command_line);
  SetUpForCrashpad(command_line);

  // Ensures that child processes have the same rules about what help features,
  // sharing feature and location share may show as the current process.
  // NOTE: this may add an --enable-features flag to the command line if not
  // already present, or append to the flag if it is.
  feature_engagement::Tracker::PropagateTestStateToChildProcess(command_line);

  SetUpFeatures(params, command_line);

  return command_line;
}

bool BrowserLauncher::LaunchProcessWithParameters(
    const base::CommandLine& command_line,
    const base::LaunchOptions& options) {
  LOG(WARNING) << "Launching lacros with command: "
               << command_line.GetCommandLineString();

  // Checks whether process_ is valid or not in order not to overwrite
  // process_.
  CHECK(!process_.IsValid());
  process_ = base::LaunchProcess(command_line, options);

  if (!process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    return false;
  }
  LOG(WARNING) << "Launched lacros-chrome with pid " << process_.Pid();
  return true;
}

}  // namespace crosapi
