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
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/files_app_launcher.h"
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

bool RemoveLacrosUserDataDir() {
  const base::FilePath lacros_data_dir = browser_util::GetUserDataDir();

  return base::PathExists(lacros_data_dir) &&
         base::DeletePathRecursively(lacros_data_dir);
}

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

  if (!CrosapiManager::IsInitialized()) {
    CHECK_IS_TEST();
  }
}

BrowserManager::~BrowserManager() {
  // Unregister, just in case the manager is destroyed before
  // OnUserSessionStarted() is called.
  if (session_manager::SessionManager::Get()) {
    session_manager::SessionManager::Get()->RemoveObserver(this);
  }

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void BrowserManager::SwitchToTab(const GURL& url,
                                 NavigateParams::PathBehavior path_behavior) {
  PerformOrEnqueue(BrowserAction::OpenUrl(
      url, crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kSwitchToTab,
      crosapi::mojom::OpenUrlFrom::kUnspecified, path_behavior));
}

void BrowserManager::InitializeAndStartIfNeeded() {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

  // Ensure this isn't run multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  PrepareLacrosPolicies(this);

  // Perform the UMA recording for the current Lacros launch mode.
  RecordLacrosLaunchMode();

  crosapi::lacros_startup_state::SetLacrosStartupState(false);
  SetState(State::UNAVAILABLE);
  browser_loader_->Unload();
  ClearLacrosData();
}

void BrowserManager::AddObserver(BrowserManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserManager::RemoveObserver(BrowserManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserManager::Shutdown() {
  shutdown_requested_ = true;
  pending_actions_.Clear();
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

void BrowserManager::OnCoreConnected(policy::CloudPolicyCore* core) {}

void BrowserManager::OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) {
  core->refresh_scheduler()->AddObserver(this);
}

void BrowserManager::OnCoreDisconnecting(policy::CloudPolicyCore* core) {}

void BrowserManager::OnCoreDestruction(policy::CloudPolicyCore* core) {
  core->RemoveObserver(this);
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

void BrowserManager::SetDeviceAccountPolicy(const std::string& policy_blob) {
  if (browser_service_.has_value()) {
    browser_service_->service->UpdateDeviceAccountPolicy(
        std::vector<uint8_t>(policy_blob.begin(), policy_blob.end()));
  }
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
      LOG(WARNING) << "lacros component image not yet available";
      pending_actions_.PushOrCancel(std::move(action),
                                    mojom::CreationResult::kBrowserNotRunning);
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
