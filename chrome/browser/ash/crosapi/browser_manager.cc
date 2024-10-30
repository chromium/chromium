// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_util.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/browser_action.h"
#include "chrome/browser/ash/crosapi/browser_launcher.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/crosapi/files_app_launcher.h"
#include "chrome/browser/ash/crosapi/test_mojo_connection_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/channel_util.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/cpp/lacros_startup_state.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-shared.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "components/crash/core/common/crash_key.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/nacl/common/buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/device_ownership_waiter.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/user_prefs/user_prefs.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/screen.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

// TODO(crbug.com/40703689): Currently, this source has log spamming
// by LOG(WARNING) for non critical errors to make it easy
// to debug and develop. Get rid of the log spamming
// when it gets stable enough.

namespace crosapi {

namespace {

// The names of the UMA metrics to track Daily LaunchMode changes.
const char kLacrosLaunchModeDaily[] = "Ash.Lacros.Launch.Mode.Daily";
const char kLacrosLaunchModeAndSourceDaily[] =
    "Ash.Lacros.Launch.ModeAndSource.Daily";

// The interval at which the daily UMA reporting function should be
// called. De-duping of events will be happening on the server side.
constexpr base::TimeDelta kDailyLaunchModeTimeDelta = base::Minutes(30);

// Pointer to the global instance of BrowserManager.
BrowserManager* g_instance = nullptr;

constexpr char kLacrosCannotLaunchNotificationID[] =
    "lacros_cannot_launch_notification_id";
constexpr char kLacrosLauncherNotifierID[] = "lacros_launcher";


void SetLaunchOnLoginPref(bool launch_on_login) {
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      browser_util::kLaunchOnLoginPref, launch_on_login);
}

bool GetLaunchOnLoginPref() {
  return ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      browser_util::kLaunchOnLoginPref);
}

bool IsKeepAliveDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kDisableLacrosKeepAliveForTesting);
}

bool IsLoginLacrosOpeningDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kDisableLoginLacrosOpening);
}

void WarnThatLacrosNotAllowedToLaunch() {
  LOG(WARNING) << "Lacros enabled but not allowed to launch";
  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kLacrosCannotLaunchNotificationID,
      /*title=*/std::u16string(),
      l10n_util::GetStringUTF16(IDS_LACROS_CANNOT_LAUNCH_MULTI_SIGNIN_MESSAGE),
      /* display_source= */ std::u16string(), GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          kLacrosLauncherNotifierID,
          ash::NotificationCatalogName::kLacrosCannotLaunch),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::RepeatingClosure()),
      gfx::kNoneIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

bool RemoveLacrosUserDataDir() {
  const base::FilePath lacros_data_dir = browser_util::GetUserDataDir();

  return base::PathExists(lacros_data_dir) &&
         base::DeletePathRecursively(lacros_data_dir);
}

// TODO(b/330659545): Investigate why we cannot run this inside
// OnUserProfileCreated.
void PrepareLacrosPolicies(BrowserManager* manager) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    LOG(ERROR) << "No primary user.";
    return;
  }

  // The lifetime of `BrowserManager` is longer than lifetime of various
  // classes, for which we register as an observer below. The RemoveObserver
  // function is therefore called in various handlers invoked by those classes
  // and not in the destructor.
  policy::CloudPolicyCore* core =
      browser_util::GetCloudPolicyCoreForUser(*user);
  if (core) {
    core->AddObserver(manager);
    if (core->refresh_scheduler()) {
      core->refresh_scheduler()->AddObserver(manager);
    }

    policy::CloudPolicyStore* store = core->store();
    if (store && store->policy_fetch_response()) {
      store->AddObserver(manager);
    }
  }

  policy::ComponentCloudPolicyService* component_policy_service =
      browser_util::GetComponentCloudPolicyServiceForUser(*user);
  if (component_policy_service) {
    component_policy_service->AddObserver(manager);
  }
}

}  // namespace

// static
BrowserManager* BrowserManager::Get() {
  return g_instance;
}

BrowserManager::BrowserManager(
    scoped_refptr<component_updater::ComponentManagerAsh> manager)
    : BrowserManager(std::make_unique<BrowserLoader>(manager),
                     g_browser_process->component_updater()) {}

BrowserManager::BrowserManager(
    std::unique_ptr<BrowserLoader> browser_loader,
    component_updater::ComponentUpdateService* update_service)
    : browser_loader_(std::move(browser_loader)) {
  DCHECK(!g_instance);
  g_instance = this;

  // Wait to query the flag until the user has entered the session. Enterprise
  // devices restart Chrome during login to apply flags. We don't want to run
  // the flag-off cleanup logic until we know we have the final flag state.
  if (session_manager::SessionManager::Get()) {
    session_manager::SessionManager::Get()->AddObserver(this);
  }

  if (CrosapiManager::IsInitialized()) {
    CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->AddObserver(this);
  } else {
    CHECK_IS_TEST();
  }

  // UserManager may not be initialized for unit tests.
  if (user_manager::UserManager::IsInitialized()) {
    user_manager_observation_.Observe(user_manager::UserManager::Get());
  }

  std::string socket_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kLacrosMojoSocketForTesting);
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
  if (session_manager::SessionManager::Get()) {
    session_manager::SessionManager::Get()->RemoveObserver(this);
  }

  // Try to kill the lacros-chrome binary.
  browser_launcher_.TriggerTerminate(/*exit_code=*/0);
  SetState(State::WAITING_FOR_MOJO_DISCONNECTED);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool BrowserManager::IsRunning() const {
  return state_ == State::RUNNING;
}

void BrowserManager::NewWindow(bool incognito,
                               bool should_trigger_session_restore) {
  int64_t target_display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  PerformOrEnqueue(BrowserAction::NewWindow(
      incognito, should_trigger_session_restore, target_display_id,
      ash::desks_util::GetActiveDeskLacrosProfileId()));
}

void BrowserManager::OpenForFullRestore(bool skip_crash_restore) {
  PerformOrEnqueue(BrowserAction::OpenForFullRestore(skip_crash_restore));
}

void BrowserManager::NewGuestWindow() {
  int64_t target_display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  PerformOrEnqueue(BrowserAction::NewGuestWindow(target_display_id));
}

void BrowserManager::Launch() {
  int64_t target_display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  PerformOrEnqueue(BrowserAction::Launch(
      target_display_id, ash::desks_util::GetActiveDeskLacrosProfileId()));
}

void BrowserManager::OpenUrl(
    const GURL& url,
    crosapi::mojom::OpenUrlFrom from,
    crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
    NavigateParams::PathBehavior path_behavior) {
  PerformOrEnqueue(
      BrowserAction::OpenUrl(url, disposition, from, path_behavior));
}

void BrowserManager::OpenCaptivePortalSignin(const GURL& url) {
  PerformOrEnqueue(BrowserAction::OpenCaptivePortalSignin(url));
}

void BrowserManager::SwitchToTab(const GURL& url,
                                 NavigateParams::PathBehavior path_behavior) {
  PerformOrEnqueue(BrowserAction::OpenUrl(
      url, crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kSwitchToTab,
      crosapi::mojom::OpenUrlFrom::kUnspecified, path_behavior));
}

void BrowserManager::CreateBrowserWithRestoredData(
    const std::vector<GURL>& urls,
    const gfx::Rect& bounds,
    const std::vector<tab_groups::TabGroupInfo>& tab_group_infos,
    ui::mojom::WindowShowState show_state,
    int32_t active_tab_index,
    int32_t first_non_pinned_tab_index,
    const std::string& app_name,
    int32_t restore_window_id,
    uint64_t lacros_profile_id) {
  PerformOrEnqueue(BrowserAction::CreateBrowserWithRestoredData(
      urls, bounds, tab_group_infos, show_state, active_tab_index,
      first_non_pinned_tab_index, app_name, restore_window_id,
      lacros_profile_id));
}

void BrowserManager::OpenProfileManager() {
  PerformOrEnqueue(BrowserAction::OpenProfileManager());
}

void BrowserManager::InitializeAndStartIfNeeded() {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

  // Ensure this isn't run multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  PrepareLacrosPolicies(this);

  // Perform the UMA recording for the current Lacros launch mode.
  RecordLacrosLaunchMode();

  // As a switch between Ash and Lacros mode requires an Ash restart plus
  // profile migration, the state will not change while the system is up.
  // At this point we are starting Lacros for the first time and with that the
  // operation mode is 'locked in'.
  const bool is_lacros_enabled = browser_util::IsLacrosEnabled();
  crosapi::lacros_startup_state::SetLacrosStartupState(is_lacros_enabled);

  if (is_lacros_enabled) {
    if (browser_util::IsLacrosAllowedToLaunch()) {
      // Start Lacros automatically on login, if
      // 1) Lacros was opened in the previous session; or
      // 2) Lacros is the primary web browser.
      //    This can be suppressed via commandline flag for testing.
      if (GetLaunchOnLoginPref() || !IsLoginLacrosOpeningDisabledForTesting()) {
        pending_actions_.Push(BrowserAction::GetActionForSessionStart());
      }
      SetState(State::MOUNTING);
      browser_loader_->Load(base::BindOnce(&BrowserManager::OnLoadComplete,
                                           weak_factory_.GetWeakPtr()));
    } else {
      SetState(State::UNAVAILABLE);
      WarnThatLacrosNotAllowedToLaunch();
    }
  } else {
    SetState(State::UNAVAILABLE);
    browser_loader_->Unload();
    ClearLacrosData();
  }
}

bool BrowserManager::GetActiveTabUrlSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kGetActiveTabUrlMinVersion;
}

void BrowserManager::GetActiveTabUrl(GetActiveTabUrlCallback callback) {
  DCHECK(GetActiveTabUrlSupported());
  browser_service_->service->GetActiveTabUrl(std::move(callback));
}

void BrowserManager::GetBrowserInformation(
    const std::string& window_unique_id,
    GetBrowserInformationCallback callback) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->desk_template_ash()
      ->GetBrowserInformation(window_unique_id, std::move(callback));
}

void BrowserManager::AddObserver(BrowserManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserManager::RemoveObserver(BrowserManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserManager::Shutdown() {
  // Lacros KeepAlive should be disabled once Shutdown() has been signalled.
  // Further calls to `UpdateKeepAliveInBrowserIfNecessary()` will no-op after
  // `shutdown_requested_` has been set.
  UpdateKeepAliveInBrowserIfNecessary(false);
  shutdown_requested_ = true;
  pending_actions_.Clear();
  browser_launcher_.Shutdown();

  // The lacros-chrome process may have already been terminated as the result of
  // a previous mojo pipe disconnection in `OnMojoDisconnected()` and not yet
  // restarted. If, on the other hand, process is alive, terminate it now.
  if (browser_launcher_.TriggerTerminate(/*exit_code=*/0)) {
    LOG(WARNING) << "Ash-chrome shutdown initiated. Terminating lacros-chrome";
    SetState(State::WAITING_FOR_PROCESS_TERMINATED);

    // Synchronously post a shutdown blocking task that waits for lacros-chrome
    // to cleanly exit. Terminate() will eventually result in a callback into
    // OnMojoDisconnected(), however this resolves asynchronously and there is a
    // risk that ash exits before this is called.
    // The 2.5s wait for a successful lacros exit stays below the 3s timeout
    // after which ash is forcefully terminated by the session_manager.
    EnsureLacrosChromeTermination(base::Milliseconds(2500));
  }
}

void BrowserManager::SetState(State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;

  for (auto& observer : observers_) {
    observer.OnStateChanged();
  }
}

std::unique_ptr<BrowserManagerScopedKeepAlive> BrowserManager::KeepAlive(
    Feature feature) {
  // Using new explicitly because BrowserManagerScopedKeepAlive's
  // constructor is private.
  return base::WrapUnique(new BrowserManagerScopedKeepAlive(this, feature));
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

void BrowserManager::Start() {
  NOTREACHED();
}

void BrowserManager::PerformAction(std::unique_ptr<BrowserAction> action) {
  BrowserAction* action_raw = action.get();  // We're `move`ing action below.
  action_raw->Perform(
      {browser_service_.value().service.get(),
       browser_service_.value().interface_version},
      base::BindOnce(&BrowserManager::OnActionPerformed,
                     weak_factory_.GetWeakPtr(), std::move(action)));
}

void BrowserManager::ClearLacrosData() {
  // Check that Lacros is not running.
  CHECK_EQ(state_, State::UNAVAILABLE);
  // Skip if Chrome is in safe mode to avoid deleting
  // user data when Lacros is disabled only temporarily.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode)) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(RemoveLacrosUserDataDir),
      base::BindOnce(&BrowserManager::OnLacrosUserDataDirRemoved,
                     weak_factory_.GetWeakPtr()));
}

void BrowserManager::OnLacrosUserDataDirRemoved(bool cleared) {
  if (!cleared) {
    // Do nothing if Lacros user data dir did not exist or could not be deleted.
    return;
  }

  LOG(WARNING) << "Lacros user data directory was cleared. Now clearing lacros "
                  "related prefs.";

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    CHECK_IS_TEST();
    return;
  }
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user);
  if (!context) {
    CHECK_IS_TEST();
    return;
  }
  PrefService* pref_service = user_prefs::UserPrefs::Get(context);

  // Clear prefs set by Lacros and stored in
  // 'standalone_browser_preferences.json' if Lacros is disabled.
  pref_service->RemoveAllStandaloneBrowserPrefs();

  // Do a one time clearing of `kUserUninstalledPreinstalledWebAppPref`. This is
  // because some users who had Lacros enabled before M114 had this pref set by
  // accident for preinstalled web apps such as Calendar or Gmail. Without
  // clearing this pref, if users disable Lacros, these apps will be considered
  // uninstalled (and cannot easily be reinstalled). Note that this means that
  // some users who intentionally uninstalled these apps on Lacros will find
  // these apps reappear until they unistall them again.
  web_app::UserUninstalledPreinstalledWebAppPrefs(pref_service).ClearAllApps();
}

void BrowserManager::OnBrowserServiceConnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id,
    mojom::BrowserService* browser_service,
    uint32_t browser_service_version) {
  if (id != crosapi_id_) {
    // This BrowserService is unrelated to this instance. Skipping.
    return;
  }

  is_terminated_ = false;

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

  DCHECK_EQ(state_, State::STARTING);
  SetState(State::RUNNING);

  // There can be a chance that keep_alive status is updated between the
  // process launching timing (where initial_keep_alive is set) and the
  // crosapi mojo connection timing (i.e., this function).
  // So, send it to lacros-chrome to update to fill the possible gap.
  UpdateKeepAliveInBrowserIfNecessary(!keep_alive_features_.empty());

  while (!pending_actions_.IsEmpty()) {
    PerformAction(pending_actions_.Pop());
    DCHECK_EQ(state_, State::RUNNING);
  }
}

void BrowserManager::OnBrowserServiceDisconnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id) {
  // No need to check CrosapiId here, because |mojo_id| is unique within a
  // process.
  if (browser_service_.has_value() && browser_service_->mojo_id == mojo_id) {
    browser_service_.reset();
  }
}

void BrowserManager::OnBrowserRelaunchRequested(CrosapiId id) {
  if (id != crosapi_id_) {
    return;
  }
  relaunch_requested_ = true;
}

void BrowserManager::OnCoreConnected(policy::CloudPolicyCore* core) {}

void BrowserManager::OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) {
  core->refresh_scheduler()->AddObserver(this);
}

void BrowserManager::OnCoreDisconnecting(policy::CloudPolicyCore* core) {}

void BrowserManager::OnCoreDestruction(policy::CloudPolicyCore* core) {
  core->RemoveObserver(this);
}

void BrowserManager::OnMojoDisconnected() {
  LOG(WARNING)
      << "Mojo to lacros-chrome is disconnected. Terminating lacros-chrome";
  SetState(State::WAITING_FOR_PROCESS_TERMINATED);
  EnsureLacrosChromeTermination(base::Seconds(5));
  for (auto& observer : observers_) {
    observer.OnMojoDisconnected();
  }
}

void BrowserManager::EnsureLacrosChromeTermination(base::TimeDelta timeout) {
  // This may be called following a synchronous termination in `Shutdown()` or
  // when the mojo pipe with the lacros-chrome process has disconnected. Early
  // return if already handling lacros-chrome termination.
  if (!browser_launcher_.IsProcessValid()) {
    return;
  }
  CHECK_EQ(state_, State::WAITING_FOR_PROCESS_TERMINATED);

  browser_service_.reset();
  crosapi_id_.reset();
  browser_launcher_.EnsureProcessTerminated(
      base::BindOnce(&BrowserManager::OnLacrosChromeTerminated,
                     weak_factory_.GetWeakPtr()),
      timeout);
}

void BrowserManager::OnLacrosChromeTerminated() {
  CHECK(state_ == State::WAITING_FOR_PROCESS_TERMINATED ||
        state_ == State::WAITING_FOR_MOJO_DISCONNECTED);
  LOG(WARNING) << "Lacros-chrome is terminated";
  is_terminated_ = true;
  SetState(State::STOPPED);

  if (relaunch_requested_) {
    pending_actions_.Push(
        BrowserAction::OpenForFullRestore(/*skip_crash_restore=*/true));
  }
  StartIfNeeded();
}

void BrowserManager::OnSessionStateChanged() {
  TRACE_EVENT0("login", "BrowserManager::OnSessionStateChanged");

  // Wait for session to become active.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  if (state_ == State::NOT_INITIALIZED) {
    InitializeAndStartIfNeeded();
  }
}

void BrowserManager::OnStoreLoaded(policy::CloudPolicyStore* store) {
  DCHECK(store);

  // A new policy got installed for the current user, so we need to pass it to
  // the Lacros browser.
  std::string policy_blob;
  if (store->policy_fetch_response()) {
    const bool success =
        store->policy_fetch_response()->SerializeToString(&policy_blob);
    DCHECK(success);
  }
  SetDeviceAccountPolicy(policy_blob);
}

void BrowserManager::OnStoreError(policy::CloudPolicyStore* store) {
  // Policy store failed, Lacros will use stale policy as well as Ash.
}

void BrowserManager::OnStoreDestruction(policy::CloudPolicyStore* store) {
  store->RemoveObserver(this);
}

void BrowserManager::OnComponentPolicyUpdated(
    const policy::ComponentPolicyMap& component_policy) {
  if (browser_service_.has_value()) {
    browser_service_->service->UpdateComponentPolicy(
        policy::CopyComponentPolicyMap(component_policy));
  }
}

void BrowserManager::OnComponentPolicyServiceDestruction(
    policy::ComponentCloudPolicyService* service) {
  service->RemoveObserver(this);
}

void BrowserManager::OnFetchAttempt(
    policy::CloudPolicyRefreshScheduler* scheduler) {
  if (browser_service_.has_value()) {
    browser_service_->service->NotifyPolicyFetchAttempt();
  }
}

void BrowserManager::OnRefreshSchedulerDestruction(
    policy::CloudPolicyRefreshScheduler* scheduler) {
  scheduler->RemoveObserver(this);
}

void BrowserManager::OnUserProfileCreated(const user_manager::User& user) {
  // Ignore if `user` is not primary.
  if (!user_manager::UserManager::Get()->IsPrimaryUser(&user)) {
    return;
  }

  // Check if Lacros is enabled for crash reporting. This must happen after the
  // primary user has been set as priamry user state is used in when evaluating
  // the correct value for IsLacrosEnabled().
  constexpr char kLacrosEnabledDataKey[] = "lacros-enabled";
  static crash_reporter::CrashKeyString<4> key(kLacrosEnabledDataKey);
  key.Set(crosapi::browser_util::IsLacrosEnabled() ? "yes" : "no");
}

void BrowserManager::OnLoadComplete(const base::FilePath& path,
                                    LacrosSelection selection,
                                    base::Version version) {
  if (shutdown_requested_) {
    LOG(ERROR) << "Load completed after Shutdown() called.";
    return;
  }
  DCHECK_EQ(state_, State::MOUNTING);

  lacros_path_ = path;
  lacros_selection_ = std::optional<LacrosSelection>(selection);
  const bool success = !path.empty();
  SetState(success ? State::STOPPED : State::UNAVAILABLE);
  // TODO(crbug.com/40801829): In the event the load operation failed, we should
  // launch the last successfully loaded image.
  for (auto& observer : observers_) {
    observer.OnLoadComplete(success, version);
  }

  StartIfNeeded();
}

void BrowserManager::StartIfNeeded() {
  if (state_ == State::STOPPED && !shutdown_requested_) {
    if (!pending_actions_.IsEmpty() || IsKeepAliveEnabled()) {
      Start();
    }
  }
}

void BrowserManager::SetDeviceAccountPolicy(const std::string& policy_blob) {
  if (browser_service_.has_value()) {
    browser_service_->service->UpdateDeviceAccountPolicy(
        std::vector<uint8_t>(policy_blob.begin(), policy_blob.end()));
  }
}

void BrowserManager::StartKeepAlive(Feature feature) {
  DCHECK(browser_util::IsLacrosEnabled());

  if (IsKeepAliveDisabledForTesting()) {
    return;
  }

  auto insertion = keep_alive_features_.insert(feature);
  // Features should never be double registered.
  // TODO(b/278643115): Replace if-statement with a (D)CHECK once browser tests
  // no longer use multiple user managers.
  if (!insertion.second) {
    CHECK_IS_TEST();
  }

  // If this is first KeepAlive instance, update the keep-alive in the browser.
  if (keep_alive_features_.size() == 1) {
    UpdateKeepAliveInBrowserIfNecessary(true);
  }
  StartIfNeeded();
}

void BrowserManager::StopKeepAlive(Feature feature) {
  keep_alive_features_.erase(feature);
  if (!IsKeepAliveEnabled()) {
    UpdateKeepAliveInBrowserIfNecessary(false);
  }
}

bool BrowserManager::IsKeepAliveEnabled() const {
  return !keep_alive_features_.empty();
}

void BrowserManager::UpdateKeepAliveInBrowserIfNecessary(bool enabled) {
  if (shutdown_requested_ || !browser_service_.has_value()) {
    // Shutdown has started or the browser is not running now. Just give up.
    return;
  }
  CHECK_GE(browser_service_->interface_version,
           crosapi::mojom::BrowserService::kUpdateKeepAliveMinVersion);
  browser_service_->service->UpdateKeepAlive(enabled);
}

void BrowserManager::SetLacrosLaunchMode() {
  LacrosLaunchMode lacros_mode;
  LacrosLaunchModeAndSource lacros_mode_and_source;

  if (!browser_util::IsAshWebBrowserEnabled()) {
    // As Ash is disabled, Lacros is the only available browser.
    lacros_mode = LacrosLaunchMode::kLacrosOnly;
    lacros_mode_and_source =
        LacrosLaunchModeAndSource::kPossiblySetByUserLacrosOnly;
  } else {
    lacros_mode = LacrosLaunchMode::kLacrosDisabled;
    lacros_mode_and_source =
        LacrosLaunchModeAndSource::kPossiblySetByUserLacrosDisabled;
  }

  crosapi::browser_util::LacrosLaunchSwitchSource source =
      crosapi::browser_util::GetLacrosLaunchSwitchSource();

  // Make sure we have always the policy loaded before we get here.
  DCHECK(source != crosapi::browser_util::LacrosLaunchSwitchSource::kUnknown);

  LacrosLaunchModeAndSource source_offset;
  if (source ==
      crosapi::browser_util::LacrosLaunchSwitchSource::kPossiblySetByUser) {
    source_offset = LacrosLaunchModeAndSource::kPossiblySetByUserLacrosDisabled;
  } else if (source ==
             crosapi::browser_util::LacrosLaunchSwitchSource::kForcedByUser) {
    source_offset = LacrosLaunchModeAndSource::kForcedByUserLacrosDisabled;
  } else {
    source_offset = LacrosLaunchModeAndSource::kForcedByPolicyLacrosDisabled;
  }

  // The states are comprised of the basic four Lacros options and the
  // source of the mode selection (By user, by Policy, by System). These
  // combinations are "nibbled together" here in one status value.
  lacros_mode_and_source = static_cast<LacrosLaunchModeAndSource>(
      static_cast<int>(source_offset) +
      static_cast<int>(lacros_mode_and_source));

  LOG(WARNING) << "Using LacrosLaunchModeAndSource "
               << static_cast<int>(lacros_mode_and_source);

  if (!lacros_mode_.has_value() || !lacros_mode_and_source_.has_value() ||
      lacros_mode != *lacros_mode_ ||
      lacros_mode_and_source != *lacros_mode_and_source_) {
    // Remember new values.
    lacros_mode_ = lacros_mode;
    lacros_mode_and_source_ = lacros_mode_and_source;
  }
}

void BrowserManager::RecordLacrosLaunchMode() {
  SetLacrosLaunchMode();

  base::UmaHistogramEnumeration("Ash.Lacros.Launch.Mode", *lacros_mode_);
  base::UmaHistogramEnumeration("Ash.Lacros.Launch.ModeAndSource",
                                *lacros_mode_and_source_);

  // Call our Daily reporting once now to make sure we have an event. If it's a
  // dupe, the server will de-dupe.
  OnDailyLaunchModeTimer();
  if (!daily_event_timer_.IsRunning()) {
    daily_event_timer_.Start(FROM_HERE, kDailyLaunchModeTimeDelta, this,
                             &BrowserManager::OnDailyLaunchModeTimer);
  }
}

void BrowserManager::PerformOrEnqueue(std::unique_ptr<BrowserAction> action) {
  if (shutdown_requested_) {
    LOG(WARNING) << "lacros-chrome is preparing for system shutdown";
    // The whole system is shutting down, so there is no point in queueing the
    // request for later.
    action->Cancel(mojom::CreationResult::kBrowserNotRunning);
    return;
  }

  switch (state_) {
    case State::UNAVAILABLE:
      LOG(ERROR) << "lacros unavailable";
      // We cannot recover from this, so there is no point in queueing the
      // request for later.
      action->Cancel(mojom::CreationResult::kBrowserNotRunning);
      return;

    case State::NOT_INITIALIZED:
    case State::MOUNTING:
      LOG(WARNING) << "lacros component image not yet available";
      pending_actions_.PushOrCancel(std::move(action),
                                    mojom::CreationResult::kBrowserNotRunning);
      return;

    case State::WAITING_FOR_MOJO_DISCONNECTED:
    case State::WAITING_FOR_PROCESS_TERMINATED:
      LOG(WARNING) << "lacros-chrome is terminating, so cannot start now";
      pending_actions_.PushOrCancel(std::move(action),
                                    mojom::CreationResult::kBrowserNotRunning);
      return;

    case State::PREPARING_FOR_LAUNCH:
      LOG(WARNING) << "lacros-chrome is preparing for launch";
      pending_actions_.PushOrCancel(std::move(action),
                                    mojom::CreationResult::kBrowserNotRunning);
      return;

    case State::STARTING:
      LOG(WARNING) << "lacros-chrome is in the process of launching";
      pending_actions_.PushOrCancel(std::move(action),
                                    mojom::CreationResult::kBrowserNotRunning);
      return;

    case State::STOPPED:
      DCHECK(!IsKeepAliveEnabled());
      DCHECK(pending_actions_.IsEmpty());
      pending_actions_.PushOrCancel(std::move(action),
                                    mojom::CreationResult::kBrowserNotRunning);
      StartIfNeeded();
      return;

    case State::RUNNING:
      if (!browser_service_.has_value()) {
        LOG(ERROR) << "BrowserService was disconnected";
        // We expect that OnMojoDisconnected will get called very soon, which
        // will transition us to STOPPED state. Hence it's okay to enqueue the
        // action.
        pending_actions_.PushOrCancel(
            std::move(action), mojom::CreationResult::kServiceDisconnected);
        return;
      }
      PerformAction(std::move(action));
      return;
  }
}

void BrowserManager::OnActionPerformed(std::unique_ptr<BrowserAction> action,
                                       bool retry) {
  if (retry) {
    PerformOrEnqueue(std::move(action));
  }
}

// Callback called when the daily event happens.
void BrowserManager::OnDailyLaunchModeTimer() {
  base::UmaHistogramEnumeration(kLacrosLaunchModeDaily, *lacros_mode_);
  base::UmaHistogramEnumeration(kLacrosLaunchModeAndSourceDaily,
                                *lacros_mode_and_source_);
}

}  // namespace crosapi
