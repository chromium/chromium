// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/browser/ash/crosapi/test_mojo_connection_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/startup/startup_switches.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

// TODO(crbug.com/1101667): Currently, this source has log spamming
// by LOG(WARNING) for non critical errors to make it easy
// to debug and develop. Get rid of the log spamming
// when it gets stable enough.

namespace crosapi {

namespace {

using LaunchParamsFromBackground = BrowserManager::LaunchParamsFromBackground;

// Pointer to the global instance of BrowserManager.
BrowserManager* g_instance = nullptr;

// The min version of BrowserService mojo interface that supports
// GetFeedbackData API.
constexpr uint32_t kGetFeedbackDataMinVersion = 6;
// The min version of BrowserService mojo interface that supports
// GetHistograms API.
constexpr uint32_t kGetHistogramsMinVersion = 7;
// The min version of BrowserService mojo interface that supports
// GetActiveTabUrl API.
constexpr uint32_t kGetActiveTabUrlMinVersion = 8;

const char kLacrosCannotLaunchNotificationID[] =
    "lacros_cannot_launch_notification_id";
const char kLacrosLauncherNotifierID[] = "lacros_launcher";

base::FilePath LacrosLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log");
}

// This method runs some work on a background thread prior to launching lacros.
// The returns struct is used by the main thread as parameters to launch Lacros.
LaunchParamsFromBackground DoLacrosBackgroundWorkPreLaunch(
    base::FilePath lacros_dir,
    bool cleared_user_data_dir) {
  LaunchParamsFromBackground params;

  // This code wipes the Lacros --user-data-dir exactly once due to an
  // incompatible account_manager change. This code can be removed when ash is
  // newer than M92, as we can then assume that all relevant users have been
  // migrated.
  //
  // First, check for Lacros metadata.
  base::FilePath metadata_path = lacros_dir.Append("metadata.json");
  JSONFileValueDeserializer deserializer(metadata_path);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> metadata =
      deserializer.Deserialize(&error_code, &error_message);

  if (browser_util::DoesMetadataSupportNewAccountManager(metadata.get())) {
    params.use_new_account_manager = true;

    // If we want to use the new account manager, and we haven't yet cleared the
    // user data dir, do so.
    if (!cleared_user_data_dir) {
      base::DeletePathRecursively(browser_util::GetUserDataDir());
    }
  }

  base::FilePath::StringType log_path = LacrosLogPath().value();

  // Delete old log file if exists.
  if (unlink(log_path.c_str()) != 0) {
    if (errno != ENOENT) {
      // unlink() failed for reason other than the file not existing.
      PLOG(ERROR) << "Failed to unlink the log file " << log_path;
      return params;
    }

    // If log file does not exist, most likely the user directory does not exist
    // either. So create it here.
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(browser_util::GetUserDataDir(),
                                          &error)) {
      LOG(ERROR) << "Failed to make directory "
                 << browser_util::GetUserDataDir()
                 << base::File::ErrorToString(error);
      return params;
    }
  }

  int fd =
      HANDLE_EINTR(open(log_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644));

  if (fd < 0) {
    PLOG(ERROR) << "Failed to get file descriptor for " << log_path;
    return params;
  }

  params.logfd = base::ScopedFD(fd);
  return params;
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
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->AddObserver(this);

  // CrosapiManager may not be initialized on unit testing.
  if (CrosapiManager::IsInitialized()) {
    CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->AddObserver(this);
  }

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
  if (CrosapiManager::IsInitialized()) {
    CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->RemoveObserver(this);
  }

  // Unregister, just in case the manager is destroyed before
  // OnUserSessionStarted() is called.
  if (session_manager::SessionManager::Get())
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

bool BrowserManager::IsRunning() const {
  return state_ == State::RUNNING;
}

bool BrowserManager::IsRunningOrWillRun() const {
  return state_ == State::RUNNING || state_ == State::STARTING ||
         state_ == State::CREATING_LOG_FILE || state_ == State::TERMINATING;
}

void BrowserManager::SetLoadCompleteCallback(LoadCompleteCallback callback) {
  // We only support one client waiting.
  DCHECK(!load_complete_callback_);
  load_complete_callback_ = std::move(callback);
}

void BrowserManager::NewWindow(bool incognito) {
  auto result = MaybeStart(
      incognito ? mojom::InitialBrowserAction::kOpenIncognitoWindow
                : mojom::InitialBrowserAction::kUseStartupPreference);
  if (result != MaybeStartResult::kRunning)
    return;

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    return;
  }
  browser_service_->service->NewWindow(incognito, base::DoNothing());
}

void BrowserManager::NewTab() {
  auto result = MaybeStart(mojom::InitialBrowserAction::kUseStartupPreference);
  if (result != MaybeStartResult::kRunning)
    return;

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    return;
  }
  browser_service_->service->NewTab(base::DoNothing());
}

void BrowserManager::RestoreTab() {
  auto result = MaybeStart(mojom::InitialBrowserAction::kRestoreLastSession);
  if (result != MaybeStartResult::kRunning)
    return;

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    return;
  }
  browser_service_->service->RestoreTab(base::DoNothing());
}

bool BrowserManager::GetFeedbackDataSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >= kGetFeedbackDataMinVersion;
}

void BrowserManager::GetFeedbackData(GetFeedbackDataCallback callback) {
  DCHECK(GetFeedbackDataSupported());
  browser_service_->service->GetFeedbackData(std::move(callback));
}

bool BrowserManager::GetHistogramsSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >= kGetHistogramsMinVersion;
}

void BrowserManager::GetHistograms(GetHistogramsCallback callback) {
  DCHECK(GetHistogramsSupported());
  browser_service_->service->GetHistograms(std::move(callback));
}

bool BrowserManager::GetActiveTabUrlSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >= kGetActiveTabUrlMinVersion;
}

void BrowserManager::GetActiveTabUrl(GetActiveTabUrlCallback callback) {
  DCHECK(GetActiveTabUrlSupported());
  browser_service_->service->GetActiveTabUrl(std::move(callback));
}

void BrowserManager::AddObserver(BrowserManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserManager::RemoveObserver(BrowserManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserManager::SetState(State state) {
  if (state_ == state)
    return;
  state_ = state;

  for (auto& observer : observers_) {
    if (state == State::TERMINATING) {
      observer.OnMojoDisconnected();
    }
    observer.OnStateChanged();
  }

  LaunchForKeepAliveIfNecessary();
}

BrowserManager::ScopedKeepAlive::~ScopedKeepAlive() {
  manager_->StopKeepAlive(feature_);
}

BrowserManager::ScopedKeepAlive::ScopedKeepAlive(BrowserManager* manager,
                                                 Feature feature)
    : manager_(manager), feature_(feature) {
  manager_->StartKeepAlive(feature_);
}

std::unique_ptr<BrowserManager::ScopedKeepAlive> BrowserManager::KeepAlive(
    Feature feature) {
  // Using new explicitly because ScopedKeepAlive's constructor is private.
  return base::WrapUnique(new ScopedKeepAlive(this, feature));
}

BrowserManager::BrowserServiceInfo::BrowserServiceInfo(
    mojo::RemoteSetElementId mojo_id,
    mojom::BrowserService* service,
    uint32_t interface_version)
    : mojo_id(mojo_id),
      service(service),
      interface_version(interface_version) {}

BrowserManager::BrowserServiceInfo::BrowserServiceInfo(
    const BrowserServiceInfo&) = default;
BrowserManager::BrowserServiceInfo&
BrowserManager::BrowserServiceInfo::operator=(const BrowserServiceInfo&) =
    default;
BrowserManager::BrowserServiceInfo::~BrowserServiceInfo() = default;

BrowserManager::MaybeStartResult BrowserManager::MaybeStart(
    mojom::InitialBrowserAction initial_browser_action) {
  if (!browser_util::IsLacrosEnabled())
    return MaybeStartResult::kNotStarted;

  if (!browser_util::IsLacrosAllowedToLaunch()) {
    std::unique_ptr<message_center::Notification> notification =
        ash::CreateSystemNotification(
            message_center::NOTIFICATION_TYPE_SIMPLE,
            kLacrosCannotLaunchNotificationID,
            /*title=*/std::u16string(),
            l10n_util::GetStringUTF16(
                IDS_LACROS_CANNOT_LAUNCH_MULTI_SIGNIN_MESSAGE),
            /* display_source= */ std::u16string(), GURL(),
            message_center::NotifierId(
                message_center::NotifierType::SYSTEM_COMPONENT,
                kLacrosLauncherNotifierID),
            message_center::RichNotificationData(),
            base::MakeRefCounted<
                message_center::HandleNotificationClickDelegate>(
                base::RepeatingClosure()),
            gfx::kNoneIcon,
            message_center::SystemNotificationWarningLevel::NORMAL);

    SystemNotificationHelper::GetInstance()->Display(*notification);
    return MaybeStartResult::kNotStarted;
  }

  if (!IsReady()) {
    LOG(WARNING) << "lacros component image not yet available";
    return MaybeStartResult::kNotStarted;
  }
  DCHECK(!lacros_path_.empty());

  if (state_ == State::TERMINATING) {
    LOG(WARNING) << "lacros-chrome is terminating, so cannot start now";
    return MaybeStartResult::kNotStarted;
  }

  if (state_ == State::CREATING_LOG_FILE || state_ == State::STARTING) {
    LOG(WARNING) << "lacros-chrome is in the process of launching";
    return MaybeStartResult::kStarting;
  }

  if (state_ == State::STOPPED) {
    // If lacros-chrome is not running, launch it.
    Start(initial_browser_action);
    return MaybeStartResult::kStarting;
  }

  return MaybeStartResult::kRunning;
}

void BrowserManager::Start(mojom::InitialBrowserAction initial_browser_action) {
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(!lacros_path_.empty());
  // Ensure we're not trying to open a window before the shelf is initialized.
  DCHECK(ChromeLauncherController::instance());

  SetState(State::CREATING_LOG_FILE);

  bool cleared_user_data_dir =
      ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
          browser_util::kClearUserDataDir1Pref);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DoLacrosBackgroundWorkPreLaunch, lacros_path_,
                     cleared_user_data_dir),
      base::BindOnce(&BrowserManager::StartWithLogFile,
                     weak_factory_.GetWeakPtr(), initial_browser_action));
}

void BrowserManager::StartWithLogFile(
    mojom::InitialBrowserAction initial_browser_action,
    LaunchParamsFromBackground params) {
  DCHECK_EQ(state_, State::CREATING_LOG_FILE);

  // If we're using the new account manager, then we must have already cleared
  // the user data dir. Record this regardless of whether a clear actually
  // happened in this invocation.
  if (params.use_new_account_manager) {
    ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
        browser_util::kClearUserDataDir1Pref, true);

    // TODO(https://crbug.com/1197220): Set the appropriate BrowserInitParams.
  }

  std::string chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = GetXdgRuntimeDir();

  // This sets the channel for Lacros.
  options.environment["CHROME_VERSION_EXTRA"] = "dev";

  std::string additional_env =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kLacrosChromeAdditionalEnv);
  base::StringPairs env_pairs;
  if (base::SplitStringIntoKeyValuePairsUsingSubstr(additional_env, '=', "####",
                                                    &env_pairs)) {
    for (const auto& env_pair : env_pairs) {
      if (!env_pair.first.empty()) {
        LOG(WARNING) << "Applying lacros env " << env_pair.first << "="
                     << env_pair.second;
        options.environment[env_pair.first] = env_pair.second;
      }
    }
  }

  options.kill_on_parent_death = true;

  // Paths are UTF-8 safe on Chrome OS.
  std::string user_data_dir = browser_util::GetUserDataDir().AsUTF8Unsafe();
  std::string crash_dir =
      browser_util::GetUserDataDir().Append("crash_dumps").AsUTF8Unsafe();

  // Pass the locale via command line instead of via LacrosInitParams because
  // the Lacros browser process needs it early in startup, before zygote fork.
  std::string locale = g_browser_process->GetApplicationLocale();

  // Static configuration should be enabled from Lacros rather than Ash. This
  // vector should only be used for dynamic configuration.
  // TODO(https://crbug.com/1145713): Remove existing static configuration.
  std::vector<std::string> argv = {chrome_path,
                                   "--ozone-platform=wayland",
                                   "--user-data-dir=" + user_data_dir,
                                   "--enable-gpu-rasterization",
                                   "--enable-oop-rasterization",
                                   "--lang=" + locale,
                                   "--enable-crashpad",
                                   "--enable-webgl-image-chromium",
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
  if (params.logfd.is_valid()) {
    // The next flag will make chrome log only via stderr. See
    // DetermineLoggingDestination in logging_chrome.cc.
    argv.push_back("--enable-logging=stderr");

    // These options will assign stdout/stderr fds to logfd in the fd table of
    // the new process.
    options.fds_to_remap.push_back(
        std::make_pair(params.logfd.get(), STDOUT_FILENO));
    options.fds_to_remap.push_back(
        std::make_pair(params.logfd.get(), STDERR_FILENO));
  }

  base::ScopedFD startup_fd = browser_util::CreateStartupData(
      environment_provider_.get(), initial_browser_action);
  if (startup_fd.is_valid()) {
    // Hardcoded to use FD 3 to make the ash-chrome's behavior more predictable.
    // Lacros-chrome should not depend on the hardcoded value though. Instead
    // it should take a look at the value passed via the command line flag.
    constexpr int kStartupDataFD = 3;
    argv.push_back(base::StringPrintf(
        "--%s=%d", chromeos::switches::kCrosStartupDataFD, kStartupDataFD));
    options.fds_to_remap.emplace_back(startup_fd.get(), kStartupDataFD);
  }

  // Set up Mojo channel.
  base::CommandLine command_line(argv);
  LOG(WARNING) << "Launching lacros with command: "
               << command_line.GetCommandLineString();

  // Prepare to invite lacros-chrome to the Mojo universe of Crosapi.
  mojo::PlatformChannel legacy_channel;
  legacy_channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  DCHECK(!legacy_crosapi_id_.has_value());
  legacy_crosapi_id_ = CrosapiManager::Get()->SendLegacyInvitation(
      legacy_channel.TakeLocalEndpoint(), base::BindOnce([]() {
        LOG(WARNING) << "Legacy Crosapi Channel disconnected";
      }));

  mojo::PlatformChannel channel;
  std::string channel_flag_value;
  channel.PrepareToPassRemoteEndpoint(&options.fds_to_remap,
                                      &channel_flag_value);
  DCHECK(!channel_flag_value.empty());
  command_line.AppendSwitchASCII(kCrosapiMojoPlatformChannelHandle,
                                 channel_flag_value);
  DCHECK(!crosapi_id_.has_value());
  // Use new Crosapi mojo connection to detect process termination always.
  // If lacros-chrome is old, the channel will be left and unused,
  // but on process termination, the socket will be closed, so the
  // disconnect_handler should be called. Note that, in that case, we should
  // carefully NOT send any messages via new Crosapi intefaces and its sub
  // interfaces, but instead, we should use the ones initiated by
  // SendLegacyInvitation just above.
  crosapi_id_ = CrosapiManager::Get()->SendInvitation(
      channel.TakeLocalEndpoint(),
      base::BindOnce(&BrowserManager::OnMojoDisconnected,
                     weak_factory_.GetWeakPtr()));

  // Create the lacros-chrome subprocess.
  base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
  lacros_launch_time_ = base::TimeTicks::Now();
  // If lacros_process_ already exists, because it does not call waitpid(2),
  // the process will never be collected.
  lacros_process_ = base::LaunchProcess(command_line, options);
  if (!lacros_process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    SetState(State::STOPPED);
    return;
  }
  SetState(State::STARTING);
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
  legacy_channel.RemoteProcessLaunchAttempted();
  channel.RemoteProcessLaunchAttempted();
}

void BrowserManager::OnBrowserServiceConnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id,
    mojom::BrowserService* browser_service,
    uint32_t browser_service_version) {
  if (id != crosapi_id_ && id != legacy_crosapi_id_) {
    // This BrowserService is unrelated to this instance. Skipping.
    return;
  }

  DCHECK_EQ(state_, State::STARTING);
  SetState(State::RUNNING);

  DCHECK(!browser_service_.has_value());
  browser_service_ =
      BrowserServiceInfo{mojo_id, browser_service, browser_service_version};
  base::UmaHistogramMediumTimes("ChromeOS.Lacros.StartTime",
                                base::TimeTicks::Now() - lacros_launch_time_);
  // Set the launch-on-login pref every time lacros-chrome successfully starts,
  // instead of once during ash-chrome shutdown, so we have the right value
  // even if ash-chrome crashes.
  SetLaunchOnLoginPref(true);
  LOG(WARNING) << "Connection to lacros-chrome is established.";
}

void BrowserManager::OnBrowserServiceDisconnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id) {
  // No need to check CrosapiId here, because |mojo_id| is unique within
  // a process.
  if (browser_service_.has_value() && browser_service_->mojo_id == mojo_id)
    browser_service_.reset();
}

void BrowserManager::OnMojoDisconnected() {
  DCHECK(state_ == State::STARTING || state_ == State::RUNNING);
  LOG(WARNING)
      << "Mojo to lacros-chrome is disconnected. Terminating lacros-chrome";

  browser_service_.reset();
  crosapi_id_.reset();
  legacy_crosapi_id_.reset();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&TerminateLacrosChrome, std::move(lacros_process_)),
      base::BindOnce(&BrowserManager::OnLacrosChromeTerminated,
                     weak_factory_.GetWeakPtr()));

  SetState(State::TERMINATING);
}

void BrowserManager::OnLacrosChromeTerminated() {
  DCHECK_EQ(state_, State::TERMINATING);
  LOG(WARNING) << "Lacros-chrome is terminated";
  SetState(State::STOPPED);
  // TODO(https://crbug.com/1109366): Restart lacros-chrome if it exits
  // abnormally (e.g. crashes). For now, assume the user meant to close it.
  SetLaunchOnLoginPref(false);
}

void BrowserManager::OnSessionStateChanged() {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

  // Wait for session to become active.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->session_state() !=
      session_manager::SessionState::ACTIVE) {
    LOG(WARNING)
        << "Session not yet active. Lacros-chrome will not be launched yet";
    return;
  }

  // Ensure this isn't run multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // May be null in tests.
  if (!component_manager_)
    return;

  DCHECK(!browser_loader_);
  browser_loader_ = std::make_unique<BrowserLoader>(component_manager_);

  // Must be checked after user session start because it depends on user type.
  if (browser_util::IsLacrosEnabled()) {
    SetState(State::LOADING);
    browser_loader_->Load(base::BindOnce(&BrowserManager::OnLoadComplete,
                                         weak_factory_.GetWeakPtr()));
  } else {
    SetState(State::UNAVAILABLE);
    browser_loader_->Unload();
  }
}

void BrowserManager::OnLoadComplete(const base::FilePath& path) {
  DCHECK_EQ(state_, State::LOADING);

  lacros_path_ = path;
  SetState(path.empty() ? State::UNAVAILABLE : State::STOPPED);
  if (load_complete_callback_) {
    const bool success = !path.empty();
    std::move(load_complete_callback_).Run(success);
  }

  if (state_ == State::STOPPED && GetLaunchOnLoginPref())
    Start(mojom::InitialBrowserAction::kUseStartupPreference);
}

void BrowserManager::SetDeviceAccountPolicy(const std::string& policy_blob) {
  environment_provider_->SetDeviceAccountPolicy(policy_blob);
  if (browser_service_.has_value()) {
    browser_service_->service->UpdateDeviceAccountPolicy(
        std::vector<uint8_t>(policy_blob.begin(), policy_blob.end()));
  }
}

LaunchParamsFromBackground::LaunchParamsFromBackground() = default;
LaunchParamsFromBackground::LaunchParamsFromBackground(
    LaunchParamsFromBackground&&) = default;
LaunchParamsFromBackground::~LaunchParamsFromBackground() = default;

void BrowserManager::StartKeepAlive(Feature feature) {
  DCHECK(keep_alive_features_.find(feature) == keep_alive_features_.end())
      << "Features should never be double registered.";

  keep_alive_features_.insert(feature);
}

void BrowserManager::StopKeepAlive(Feature feature) {
  keep_alive_features_.erase(feature);
  if (keep_alive_features_.empty())
    UnlauchForKeepAlive();
}

void BrowserManager::LaunchForKeepAliveIfNecessary() {
  if (state_ == State::STOPPED && !keep_alive_features_.empty()) {
    CHECK(browser_util::IsLacrosEnabled());
    CHECK(browser_util::IsLacrosAllowedToLaunch());
    Start(mojom::InitialBrowserAction::kDoNotOpenWindow);
  }
}

void BrowserManager::UnlauchForKeepAlive() {
  // TODO(https://crbug.com/1194187): Implement this.
}

}  // namespace crosapi
