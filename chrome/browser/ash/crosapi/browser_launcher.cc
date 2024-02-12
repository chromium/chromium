// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
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
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
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
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "ui/base/ui_base_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_switches.h"
#endif

namespace crosapi {

namespace {

using LaunchParamsFromBackground = BrowserLauncher::LaunchParamsFromBackground;
using LaunchParams = BrowserLauncher::LaunchParams;
using LaunchResults = BrowserLauncher::LaunchResults;

base::FilePath LacrosPostLoginLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log");
}

base::FilePath LacrosCrashDumpDirectory() {
  return BrowserLauncher::LacrosLogDirectory().Append("Crash Reports");
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

#if defined(USE_CRAS)
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

void SetUpEnvironment(browser_util::LacrosSelection lacros_selection,
                      base::LaunchOptions& options) {
  // If Ash is an unknown channel then this is not a production build and we
  // should be using an unknown channel for Lacros as well. This prevents Lacros
  // from picking up Finch experiments.
  version_info::Channel update_channel = version_info::Channel::UNKNOWN;
  if (chrome::GetChannel() != version_info::Channel::UNKNOWN) {
    update_channel =
        browser_util::GetLacrosSelectionUpdateChannel(lacros_selection);
    // If we don't have channel information, we default to the "dev" channel.
    if (update_channel == version_info::Channel::UNKNOWN) {
      update_channel = browser_util::kLacrosDefaultChannel;
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

void SetUpLogging(bool launching_at_login_screen,
                  std::optional<int> logfd,
                  LaunchParams& parameters) {
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
      // TODO(crbug.com/1371493): Remove after fix.
      "wayland_window_drag_controller=1,wayland_data_source=1,tab_drag_"
      // TODO(crbug.com/1472682): Remove after fix.
      "controller=1, wayland_data_drag_controller=1");

  if (launching_at_login_screen &&
      !current_command_line->HasSwitch(switches::kDisableLoggingRedirect)) {
    // Redirects logs to cryptohome after login on non-test images.
    parameters.command_line.AppendSwitchASCII(
        chromeos::switches::kCrosPostLoginLogFile,
        LacrosPostLoginLogPath().value());
  }

  // These options will assign stdout/stderr fds to logfd in the fd table of
  // the new process.
  parameters.options.fds_to_remap.push_back(
      std::make_pair(logfd.value(), STDOUT_FILENO));
  parameters.options.fds_to_remap.push_back(
      std::make_pair(logfd.value(), STDERR_FILENO));
}

// Sets up switches and arguments of command line for startup and post-login
// data.
void SetUpForStartupData(std::optional<int> startup_fd,
                         std::optional<int> read_pipe_fd,
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

  // If at login screen, open an anonymous pipe to pass post-login parameters to
  // Lacros later on.
  if (read_pipe_fd) {
    // Pass the read side of the pipe to the Lacros process.
    constexpr int kPostLoginDataFD = 4;
    parameters.command_line.AppendSwitchASCII(
        chromeos::switches::kCrosPostLoginDataFD,
        base::NumberToString(kPostLoginDataFD));
    parameters.options.fds_to_remap.emplace_back(read_pipe_fd.value(),
                                                 kPostLoginDataFD);
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

  if (params.enable_shared_components_dir) {
    // Passes a flag to enable using a location shared across users for browser
    // components.
    parameters.command_line.AppendSwitch(
        switches::kEnableLacrosSharedComponentsDir);
  }

  if (params.enable_fork_zygotes_at_login_screen) {
    parameters.command_line.AppendSwitch(
        switches::kEnableLacrosForkZygotesAtLoginScreen);
  }
}

}  // namespace

// To be sure the lacros is running with neutral thread type.
class LacrosThreadTypeDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  void RunAsyncSafe() override {
    // TODO(crbug.com/1289736): Currently, this is causing some deadlock issue.
    // It looks like inside the function, we seem to call async unsafe API.
    // For the mitigation, disabling this temporarily.
    // We should revisit here, and see the impact of performance.
    // SetCurrentThreadType() needs file I/O on /proc and /sys.
    // base::ScopedAllowBlocking allow_blocking;
    // base::PlatformThread::SetCurrentThreadType(
    //     base::ThreadType::kDefault);
  }
};

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

std::optional<LaunchResults> BrowserLauncher::LaunchProcess(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    bool launching_at_login_screen,
    browser_util::LacrosSelection lacros_selection,
    base::OnceClosure mojo_disconnection_cb,
    bool is_keep_alive_enabled) {
  LOG(WARNING) << "Starting lacros-chrome launching at "
               << chrome_path.MaybeAsASCII();
  // Creates FD for startup.
  // For backward compatibility, we want to pass all the parameters at
  // startup if we're not launching at login screen.
  // Vice versa, if we're launching at login screen, we want to split
  // the parameters in pre-login and post-login.
  base::ScopedFD startup_fd = browser_util::CreateStartupData(
      &environment_provider_,
      browser_util::InitialBrowserAction(
          mojom::InitialBrowserAction::kDoNotOpenWindow),
      !is_keep_alive_enabled, lacros_selection, !launching_at_login_screen);

  LaunchResults launch_results;
  // Creates a pipe between FDs when Lacros is launching at login screen.
  base::ScopedFD read_pipe_fd;
  if (launching_at_login_screen) {
    CHECK(base::CreatePipe(&read_pipe_fd, &postlogin_pipe_fd_));
  }

  // Sets up Mojo channel.
  // Uses new Crosapi mojo connection to detect process termination always.
  mojo::PlatformChannel channel;
  launch_results.crosapi_id = CrosapiManager::Get()->SendInvitation(
      channel.TakeLocalEndpoint(), std::move(mojo_disconnection_cb));

  // Initialize command line and options for launching Lacros.
  // Do NOT include any codes with side effects because we just set up command
  // line and options in this function. Do NOT modify LaunchParams outside of
  // `CreateLaunchParams`.
  LaunchParams parameters = CreateLaunchParams(
      chrome_path, params, launching_at_login_screen,
      startup_fd.is_valid() ? std::optional(startup_fd.get()) : std::nullopt,
      read_pipe_fd.is_valid() ? std::optional(read_pipe_fd.get())
                              : std::nullopt,
      channel, lacros_selection);

  base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
  launch_results.lacros_launch_time = base::TimeTicks::Now();

  bool success = LaunchProcessWithParameters(parameters);
  channel.RemoteProcessLaunchAttempted();

  return success ? std::make_optional(std::move(launch_results)) : std::nullopt;
}

void BrowserLauncher::ResumeLaunch() {
  CHECK(postlogin_pipe_fd_.is_valid());
  // Write post-login parameters into the anonymous pipe.
  bool write_success = browser_util::WritePostLoginData(
      postlogin_pipe_fd_.get(), &environment_provider_,
      browser_util::InitialBrowserAction(
          mojom::InitialBrowserAction::kDoNotOpenWindow));
  PCHECK(write_success);
  postlogin_pipe_fd_.reset();
}

void BrowserLauncher::SetLastPolicyFetchAttemptTimestamp(
    base::Time last_refresh) {
  environment_provider_.SetLastPolicyFetchAttemptTimestamp(last_refresh);
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

const base::Process& BrowserLauncher::GetProcessForTesting() {
  return process_;
}

bool BrowserLauncher::LaunchProcessForTesting(const LaunchParams& parameters) {
  return LaunchProcessWithParameters(parameters);
}

void BrowserLauncher::SetUpAdditionalParametersForTesting(
    LaunchParamsFromBackground& params,
    LaunchParams& parameters) {
  SetUpLacrosAdditionalParameters(params, parameters);
}

LaunchParams BrowserLauncher::CreateLaunchParams(
    const base::FilePath& chrome_path,
    const LaunchParamsFromBackground& params,
    bool launching_at_login_screen,
    std::optional<int> startup_fd,
    std::optional<int> read_pipe_fd,
    mojo::PlatformChannel& channel,
    browser_util::LacrosSelection lacros_selection) {
  // Static configuration should be enabled from Lacros rather than Ash. This
  // vector should only be used for dynamic configuration.
  // TODO(https://crbug.com/1145713): Remove existing static configuration.
  LaunchParams parameters(CreateCommandLine(chrome_path),
                          CreateLaunchOptions());

  SetUpEnvironment(lacros_selection, parameters.options);
  SetUpForDevMode(parameters.command_line);
#if BUILDFLAG(ENABLE_NACL)
  SetUpForNacl(parameters.command_line);
#endif
  SetUpLacrosAdditionalParameters(params, parameters);
  SetUpForGpu(parameters.command_line);
  SetUpLogging(launching_at_login_screen,
               params.logfd.is_valid() ? std::optional(params.logfd.get())
                                       : std::nullopt,
               parameters);
  SetUpForStartupData(startup_fd, read_pipe_fd, parameters);
  SetUpForMojo(channel, parameters);
  SetUpForCrashpad(parameters.command_line);

  // Ensures that child processes have the same rules about what help features,
  // sharing feature and location share may show as the current process.
  // NOTE: this may add an --enable-features flag to the command line if not
  // already present, or append to the flag if it is.
  feature_engagement::Tracker::PropagateTestStateToChildProcess(
      parameters.command_line);

  SetUpFeatures(params, parameters);

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
