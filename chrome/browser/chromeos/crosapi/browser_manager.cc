// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_manager.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crosapi/ash_chrome_service_impl.h"
#include "chrome/browser/chromeos/crosapi/browser_loader.h"
#include "chrome/browser/chromeos/crosapi/browser_util.h"
#include "chrome/browser/chromeos/crosapi/environment_provider.h"
#include "chrome/browser/chromeos/crosapi/test_mojo_connection_manager.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"

// TODO(crbug.com/1101667): Currently, this source has log spamming
// by LOG(WARNING) for non critical errors to make it easy
// to debug and develop. Get rid of the log spamming
// when it gets stable enough.

namespace crosapi {

namespace {

// Pointer to the global instance of BrowserManager.
BrowserManager* g_instance = nullptr;

base::FilePath LacrosLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log");
}

base::ScopedFD CreateLogFile() {
  base::FilePath::StringType log_path = LacrosLogPath().value();

  // Delete old log file if exists.
  if (unlink(log_path.c_str()) != 0) {
    if (errno != ENOENT) {
      // unlink() failed for reason other than the file not existing.
      PLOG(ERROR) << "Failed to unlink the log file " << log_path;
      return base::ScopedFD();
    }

    // If log file does not exist, most likely the user directory does not exist
    // either. So create it here.
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(browser_util::GetUserDataDir(),
                                          &error)) {
      LOG(ERROR) << "Failed to make directory "
                 << browser_util::GetUserDataDir()
                 << base::File::ErrorToString(error);
      return base::ScopedFD();
    }
  }

  int fd =
      HANDLE_EINTR(open(log_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644));

  if (fd < 0) {
    PLOG(ERROR) << "Failed to get file descriptor for " << log_path;
    return base::ScopedFD();
  }

  return base::ScopedFD(fd);
}

std::string GetXdgRuntimeDir() {
  // If ash-chrome was given an environment variable, use it.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string xdg_runtime_dir;
  if (env->GetVar("XDG_RUNTIME_DIR", &xdg_runtime_dir))
    return xdg_runtime_dir;

  // Otherwise provide the default for Chrome OS devices.
  return "/run/chrome";
}

void TerminateLacrosChrome(base::Process process) {
  // Here, lacros-chrome process may crashed, or be in the shutdown procedure.
  // Give some amount of time for the collection. In most cases,
  // this wait captures the process termination.
  constexpr base::TimeDelta kGracefulShutdownTimeout =
      base::TimeDelta::FromSeconds(5);
  if (process.WaitForExitWithTimeout(kGracefulShutdownTimeout, nullptr))
    return;

  // Here, the process is not yet terminated.
  // This happens if some critical error happens on the mojo connection,
  // while both ash-chrome and lacros-chrome are still alive.
  // Terminate the lacros-chrome.
  bool success = process.Terminate(/*exit_code=*/0, /*wait=*/true);
  LOG_IF(ERROR, !success) << "Failed to terminate the lacros-chrome.";
}

void SetLaunchOnLoginPref(bool launch_on_login) {
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      browser_util::kLaunchOnLoginPref, launch_on_login);
}

bool GetLaunchOnLoginPref() {
  return ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      browser_util::kLaunchOnLoginPref);
}

}  // namespace

// static
BrowserManager* BrowserManager::Get() {
  return g_instance;
}

BrowserManager::BrowserManager(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : component_manager_(manager),
      environment_provider_(std::make_unique<EnvironmentProvider>()) {
  DCHECK(!g_instance);
  g_instance = this;

  // Wait to query the flag until the user has entered the session. Enterprise
  // devices restart Chrome during login to apply flags. We don't want to run
  // the flag-off cleanup logic until we know we have the final flag state.
  session_manager::SessionManager::Get()->AddObserver(this);

  std::string socket_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kLacrosMojoSocketForTesting);
  if (!socket_path.empty()) {
    test_mojo_connection_manager_ =
        std::make_unique<crosapi::TestMojoConnectionManager>(
            base::FilePath(socket_path));
  }
}

BrowserManager::~BrowserManager() {
  // Unregister, just in case the manager is destroyed before
  // OnUserSessionStarted() is called.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // Try to kill the lacros-chrome binary.
  if (lacros_process_.IsValid())
    lacros_process_.Terminate(/*ignored=*/0, /*wait=*/false);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool BrowserManager::IsReady() const {
  return state_ != State::NOT_INITIALIZED && state_ != State::LOADING &&
         state_ != State::UNAVAILABLE;
}

void BrowserManager::SetLoadCompleteCallback(LoadCompleteCallback callback) {
  load_complete_callback_ = std::move(callback);
}

void BrowserManager::NewWindow() {
  if (!browser_util::IsLacrosAllowed())
    return;

  if (!IsReady()) {
    LOG(WARNING) << "lacros component image not yet available";
    return;
  }
  DCHECK(!lacros_path_.empty());

  if (state_ == State::TERMINATING) {
    LOG(WARNING) << "lacros-chrome is terminating, so cannot start now";
    return;
  }

  if (state_ == State::CREATING_LOG_FILE) {
    LOG(WARNING) << "lacros-chrome is in the process of launching";
    return;
  }

  if (state_ == State::STOPPED) {
    // If lacros-chrome is not running, launch it.
    Start();
    return;
  }

  DCHECK(lacros_chrome_service_.is_connected());
  lacros_chrome_service_->NewWindow(base::DoNothing());
}

void BrowserManager::Start() {
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(!lacros_path_.empty());
  // Ensure we're not trying to open a window before the shelf is initialized.
  DCHECK(ChromeLauncherController::instance());

  state_ = State::CREATING_LOG_FILE;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateLogFile),
      base::BindOnce(&BrowserManager::StartWithLogFile,
                     weak_factory_.GetWeakPtr()));
}

void BrowserManager::StartWithLogFile(base::ScopedFD logfd) {
  DCHECK_EQ(state_, State::CREATING_LOG_FILE);

  std::string chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = GetXdgRuntimeDir();

  std::string api_key;
  if (google_apis::HasAPIKeyConfigured())
    api_key = google_apis::GetAPIKey();
  else
    api_key = google_apis::GetNonStableAPIKey();
  options.environment["GOOGLE_API_KEY"] = api_key;
  options.environment["GOOGLE_DEFAULT_CLIENT_ID"] =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_MAIN);
  options.environment["GOOGLE_DEFAULT_CLIENT_SECRET"] =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_MAIN);

  options.kill_on_parent_death = true;

  // Paths are UTF-8 safe on Chrome OS.
  std::string user_data_dir = browser_util::GetUserDataDir().AsUTF8Unsafe();
  std::string crash_dir =
      browser_util::GetUserDataDir().Append("crash_dumps").AsUTF8Unsafe();

  std::vector<std::string> argv = {chrome_path,
                                   "--ozone-platform=wayland",
                                   "--user-data-dir=" + user_data_dir,
                                   "--enable-gpu-rasterization",
                                   "--enable-oop-rasterization",
                                   "--lang=en-US",
                                   "--enable-crashpad",
                                   "--breakpad-dump-location=" + crash_dir};

  // CrAS is the default audio server in Chrome OS.
  if (base::SysInfo::IsRunningOnChromeOS())
    argv.push_back("--use-cras");

  std::string additional_flags =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kLacrosChromeAdditionalArgs);
  std::vector<std::string> delimited_flags = base::SplitStringUsingSubstr(
      additional_flags, "####", base::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  for (const std::string& flag : delimited_flags) {
    argv.push_back(flag);
  }

  // If logfd is valid, enable logging and redirect stdout/stderr to logfd.
  if (logfd.is_valid()) {
    // The next flag will make chrome log only via stderr. See
    // DetermineLoggingDestination in logging_chrome.cc.
    argv.push_back("--enable-logging=stderr");

    // These options will assign stdout/stderr fds to logfd in the fd table of
    // the new process.
    options.fds_to_remap.push_back(std::make_pair(logfd.get(), STDOUT_FILENO));
    options.fds_to_remap.push_back(std::make_pair(logfd.get(), STDERR_FILENO));
  }

  // Set up Mojo channel.
  base::CommandLine command_line(argv);
  LOG(WARNING) << "Launching lacros with command: "
               << command_line.GetCommandLineString();
  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&options, &command_line);

  // TODO(crbug.com/1124490): Support multiple mojo connections from lacros.
  lacros_chrome_service_ = browser_util::SendMojoInvitationToLacrosChrome(
      environment_provider_.get(), channel.TakeLocalEndpoint(),
      base::BindOnce(&BrowserManager::OnMojoDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&BrowserManager::OnAshChromeServiceReceiverReceived,
                     weak_factory_.GetWeakPtr()));

  // Create the lacros-chrome subprocess.
  base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
  // If lacros_process_ already exists, because it does not call waitpid(2),
  // the process will never be collected.
  lacros_process_ = base::LaunchProcess(command_line, options);
  if (!lacros_process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    state_ = State::STOPPED;
    return;
  }
  state_ = State::STARTING;
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
  channel.RemoteProcessLaunchAttempted();
}

void BrowserManager::OnAshChromeServiceReceiverReceived(
    mojo::PendingReceiver<crosapi::mojom::AshChromeService> pending_receiver) {
  DCHECK_EQ(state_, State::STARTING);
  ash_chrome_service_ =
      std::make_unique<AshChromeServiceImpl>(std::move(pending_receiver));
  state_ = State::RUNNING;
  // Set the launch-on-login pref every time lacros-chrome successfully starts,
  // instead of once during ash-chrome shutdown, so we have the right value
  // even if ash-chrome crashes.
  SetLaunchOnLoginPref(true);
  LOG(WARNING) << "Connection to lacros-chrome is established.";
}

void BrowserManager::OnMojoDisconnected() {
  DCHECK(state_ == State::STARTING || state_ == State::RUNNING);
  LOG(WARNING)
      << "Mojo to lacros-chrome is disconnected. Terminating lacros-chrome";
  state_ = State::TERMINATING;

  lacros_chrome_service_.reset();
  ash_chrome_service_ = nullptr;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&TerminateLacrosChrome, std::move(lacros_process_)),
      base::BindOnce(&BrowserManager::OnLacrosChromeTerminated,
                     weak_factory_.GetWeakPtr()));
}

void BrowserManager::OnLacrosChromeTerminated() {
  DCHECK_EQ(state_, State::TERMINATING);
  LOG(WARNING) << "Lacros-chrome is terminated";
  state_ = State::STOPPED;
  // TODO(https://crbug.com/1109366): Restart lacros-chrome if it exits
  // abnormally (e.g. crashes). For now, assume the user meant to close it.
  SetLaunchOnLoginPref(false);
}

void BrowserManager::OnSessionStateChanged() {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

  // Wait for session to become active.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->session_state() != session_manager::SessionState::ACTIVE)
    return;

  // Ensure this isn't run multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // Must be checked after user session start because it depends on user type.
  if (!browser_util::IsLacrosAllowed())
    return;

  // May be null in tests.
  if (!component_manager_)
    return;

  DCHECK(!browser_loader_);
  browser_loader_ = std::make_unique<BrowserLoader>(component_manager_);
  if (chromeos::features::IsLacrosSupportEnabled()) {
    state_ = State::LOADING;
    browser_loader_->Load(base::BindOnce(&BrowserManager::OnLoadComplete,
                                         weak_factory_.GetWeakPtr()));
  } else {
    state_ = State::UNAVAILABLE;
    browser_loader_->Unload();
  }
}

void BrowserManager::OnLoadComplete(const base::FilePath& path) {
  DCHECK_EQ(state_, State::LOADING);

  lacros_path_ = path;
  state_ = path.empty() ? State::UNAVAILABLE : State::STOPPED;
  if (load_complete_callback_) {
    const bool success = !path.empty();
    std::move(load_complete_callback_).Run(success);
  }

  if (state_ == State::STOPPED && GetLaunchOnLoginPref()) {
    Start();
  }
}

}  // namespace crosapi
