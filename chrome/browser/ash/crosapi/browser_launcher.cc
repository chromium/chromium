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
#include "base/system/sys_info.h"
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
#include "components/nacl/common/nacl_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "ui/base/ui_base_switches.h"

namespace crosapi {

namespace {

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

using LaunchParamsFromBackground = BrowserLauncher::LaunchParamsFromBackground;

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

std::vector<std::string> BrowserLauncher::InitializeArgv(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    bool launching_at_login_screen,
    std::optional<int> startup_data_fd,
    std::optional<int> postlogin_data_fd) {
  // Paths are UTF-8 safe on Chrome OS.
  std::string user_data_dir = browser_util::GetUserDataDir().AsUTF8Unsafe();
  std::string crash_dir = LacrosCrashDumpDirectory().AsUTF8Unsafe();

  // Passes the locale via command line instead of via LacrosInitParams because
  // the Lacros browser process needs it early in startup, before zygote fork.
  std::string locale = g_browser_process->GetApplicationLocale();

  // Static configuration should be enabled from Lacros rather than Ash. This
  // vector should only be used for dynamic configuration.
  // TODO(https://crbug.com/1145713): Remove existing static configuration.
  std::vector<std::string> argv = {chrome_path.MaybeAsASCII(),
                                   "--ozone-platform=wayland",
                                   "--user-data-dir=" + user_data_dir,
                                   "--enable-gpu-rasterization",
                                   "--lang=" + locale,
                                   "--enable-webgl-image-chromium",
                                   "--breakpad-dump-location=" + crash_dir};

  // CrAS is the default audio server in Chrome OS.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    argv.push_back("--use-cras");
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSystemDevMode)) {
    argv.push_back("--system-developer-mode");
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowRAInDevMode)) {
    argv.push_back("--allow-ra-in-dev-mode");
  }

#if BUILDFLAG(ENABLE_NACL)
  // This switch is forwarded to nacl_helper and is needed before zygote fork.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVerboseLoggingInNacl)) {
    argv.push_back("--verbose-logging-in-nacl=" +
                   base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                       switches::kVerboseLoggingInNacl));
  }
#endif

  std::string additional_flags =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kLacrosChromeAdditionalArgs);
  std::vector<base::StringPiece> delimited_flags =
      base::SplitStringPieceUsingSubstr(additional_flags, "####",
                                        base::TRIM_WHITESPACE,
                                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& flag : delimited_flags) {
    argv.emplace_back(flag);
  }

  argv.insert(argv.end(), params.lacros_additional_args.begin(),
              params.lacros_additional_args.end());

  // Forward flag for zero copy video capture to Lacros if it is enabled.
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    argv.emplace_back(
        base::StringPrintf("--%s", switches::kVideoCaptureUseGpuMemoryBuffer));
  }

  // If logfd is valid, enables logging and redirect stdout/stderr to logfd.
  if (params.logfd.is_valid()) {
    // The next flag will make chrome log only via stderr. See
    // DetermineLoggingDestination in logging_chrome.cc.
    argv.push_back("--enable-logging=stderr");

    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kLoggingLevel)) {
      argv.push_back(base::StringPrintf(
          "--%s=%s", switches::kLoggingLevel,
          command_line->GetSwitchValueASCII(switches::kLoggingLevel).c_str()));
    }

    argv.push_back(std::string("--vmodule=")
                   // TODO(crbug.com/1371493): Remove after fix.
                   + "wayland_window_drag_controller=1,wayland_data_source=1" +
                   ",tab_drag_controller=1" +
                   // TODO(crbug.com/1472682): Remove after fix.
                   ",wayland_data_drag_controller=1");

    if (launching_at_login_screen &&
        !command_line->HasSwitch(switches::kDisableLoggingRedirect)) {
      // Redirects logs to cryptohome after login on non-test images.
      argv.push_back(base::StringPrintf(
          "--%s=%s", chromeos::switches::kCrosPostLoginLogFile,
          LacrosPostLoginLogPath().value().c_str()));
    }
  }

  // TODO(mayukoaiba): This part and the one in BrowserManager depending on
  // whether `startup_fd` is valid or not need to be consistent.
  if (startup_data_fd) {
    argv.push_back(base::StringPrintf("--%s=%d",
                                      chromeos::switches::kCrosStartupDataFD,
                                      startup_data_fd.value()));
  }

  // TODO(mayukoaiba): This part and the one in BrowserManager depending on
  // `launching_at_login_screen` need to be consistent.
  if (postlogin_data_fd) {
    CHECK(launching_at_login_screen);
    argv.push_back(base::StringPrintf("--%s=%d",
                                      chromeos::switches::kCrosPostLoginDataFD,
                                      postlogin_data_fd.value()));
  }

  return argv;
}

base::CommandLine BrowserLauncher::InitializeParameters(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    bool launching_at_login_screen,
    std::optional<int> startup_data_fd,
    std::optional<int> postlogin_data_fd,
    std::string_view channel_flag_value) {
  // TODO(mayukoaiba): The process of initializeing command_line is written
  // flat. And I have to break it down into functions based on their respective
  // functionalities.
  base::CommandLine command_line(
      InitializeArgv(chrome_path, params, launching_at_login_screen,
                     startup_data_fd, postlogin_data_fd));

  CHECK(!channel_flag_value.empty());
  command_line.AppendSwitchASCII(kCrosapiMojoPlatformChannelHandle,
                                 channel_flag_value);

  if (crash_reporter::IsCrashpadEnabled()) {
    command_line.AppendSwitch(switches::kEnableCrashpad);
  }

  // Ensures that child processes have the same rules about what help features,
  // sharing feature and location share may show as the current process.
  // NOTE: this may add an --enable-features flag to the command line if not
  // already present, or append to the flag if it is.
  feature_engagement::Tracker::PropagateTestStateToChildProcess(command_line);

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
