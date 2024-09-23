// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/system_tray.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/notifications/update_required_notification.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/chromeos/devicetype_utils.h"

namespace policy {

namespace {

using ::ash::UpdateEngineClient;
using MinimumVersionRequirement =
    MinimumVersionPolicyHandler::MinimumVersionRequirement;

const int kOneWeekEolNotificationInDays = 7;

PrefService* local_state() {
  return g_browser_process->local_state();
}

MinimumVersionPolicyHandler::NetworkStatus GetCurrentNetworkStatus() {
  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* current_network =
      network_state_handler->DefaultNetwork();
  if (!current_network || !current_network->IsConnectedState())
    return MinimumVersionPolicyHandler::NetworkStatus::kOffline;
  if (network_state_handler->default_network_is_metered())
    return MinimumVersionPolicyHandler::NetworkStatus::kMetered;
  return MinimumVersionPolicyHandler::NetworkStatus::kAllowed;
}

void OpenNetworkSettings() {
  ash::SystemTray::Get()->ShowNetworkDetailedViewBubble();
}

void OpenEnterpriseInfoPage() {
  SystemTrayClientImpl::Get()->ShowEnterpriseInfo();
}

std::string GetEnterpriseManager() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetEnterpriseDomainManager();
}

BuildState* GetBuildState() {
  return g_browser_process->GetBuildState();
}

int GetDaysRounded(base::TimeDelta time) {
  return base::ClampRound(time / base::Days(1));
}

// Overrides the relaunch notification style to required and configures the
// relaunch deadline according to the deadline.
void OverrideRelaunchNotification(base::Time deadline) {
  UpgradeDetector* upgrade_detector = UpgradeDetector::GetInstance();
  upgrade_detector->OverrideRelaunchNotificationToRequired(true);
  upgrade_detector->OverrideHighAnnoyanceDeadline(deadline);
}

// Resets the overridden relaunch notification style and deadline.
void ResetRelaunchNotification() {
  UpgradeDetector* upgrade_detector = UpgradeDetector::GetInstance();
  upgrade_detector->ResetOverriddenDeadline();
  upgrade_detector->OverrideRelaunchNotificationToRequired(false);
}

}  // namespace

const char MinimumVersionPolicyHandler::kRequirements[] = "requirements";
const char MinimumVersionPolicyHandler::kChromeOsVersion[] = "chromeos_version";
const char MinimumVersionPolicyHandler::kWarningPeriod[] = "warning_period";
const char MinimumVersionPolicyHandler::kEolWarningPeriod[] =
    "aue_warning_period";
const char MinimumVersionPolicyHandler::kUnmanagedUserRestricted[] =
    "unmanaged_user_restricted";

MinimumVersionRequirement::MinimumVersionRequirement(
    const base::Version version,
    const base::TimeDelta warning,
    const base::TimeDelta eol_warning)
    : minimum_version_(version),
      warning_time_(warning),
      eol_warning_time_(eol_warning) {}

std::unique_ptr<MinimumVersionRequirement>
MinimumVersionRequirement::CreateInstanceIfValid(
    const base::Value::Dict& dict) {
  const std::string* version = dict.FindString(kChromeOsVersion);
  if (!version)
    return nullptr;
  base::Version minimum_version(*version);
  if (!minimum_version.IsValid())
    return nullptr;
  auto warning = dict.FindInt(kWarningPeriod);
  base::TimeDelta warning_time =
      base::Days(warning.has_value() ? warning.value() : 0);
  auto eol_warning = dict.FindInt(kEolWarningPeriod);
  base::TimeDelta eol_warning_time =
      base::Days(eol_warning.has_value() ? eol_warning.value() : 0);
  return std::make_unique<MinimumVersionRequirement>(
      minimum_version, warning_time, eol_warning_time);
}

int MinimumVersionRequirement::Compare(
    const MinimumVersionRequirement* other) const {
  const int version_compare = version().CompareTo(other->version());
  if (version_compare != 0)
    return version_compare;
  if (warning() != other->warning())
    return (warning() > other->warning() ? 1 : -1);
  if (eol_warning() != other->eol_warning())
    return (eol_warning() > other->eol_warning() ? 1 : -1);
  return 0;
}

MinimumVersionPolicyHandler::MinimumVersionPolicyHandler(
    Delegate* delegate,
    ash::CrosSettings* cros_settings)
    : delegate_(delegate),
      cros_settings_(cros_settings),
      clock_(base::DefaultClock::GetInstance()) {
  policy_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceMinimumVersion,
      base::BindRepeating(&MinimumVersionPolicyHandler::OnPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  // Fire it once so we're sure we get an invocation on startup.
  OnPolicyChanged();
}

MinimumVersionPolicyHandler::~MinimumVersionPolicyHandler() {
  GetBuildState()->RemoveObserver(this);
  StopObservingNetwork();
  UpdateEngineClient::Get()->RemoveObserver(this);
}

void MinimumVersionPolicyHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MinimumVersionPolicyHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MinimumVersionPolicyHandler::CurrentVersionSatisfies(
    const MinimumVersionRequirement& requirement) const {
  base::Version platform_version(delegate_->GetCurrentVersion());
  if (platform_version.IsValid())
    return delegate_->GetCurrentVersion().CompareTo(requirement.version()) >= 0;
  return true;
}

bool MinimumVersionPolicyHandler::IsPolicyRestrictionAppliedForUser() const {
  return delegate_->IsUserEnterpriseManaged() || unmanaged_user_restricted_;
}

//  static
void MinimumVersionPolicyHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kUpdateRequiredTimerStartTime,
                             base::Time());
  registry->RegisterTimeDeltaPref(prefs::kUpdateRequiredWarningPeriod,
                                  base::TimeDelta());
}

bool MinimumVersionPolicyHandler::ShouldShowUpdateRequiredEolBanner() const {
  return !RequirementsAreSatisfied() && IsPolicyRestrictionAppliedForUser() &&
         eol_reached_;
}

bool MinimumVersionPolicyHandler::IsDeadlineTimerRunningForTesting() const {
  return update_required_deadline_timer_.IsRunning();
}

bool MinimumVersionPolicyHandler::IsPolicyApplicable() {
  bool device_managed = delegate_->IsDeviceEnterpriseManaged();
  bool is_kiosk = delegate_->IsKioskMode();
  return device_managed && !is_kiosk;
}

void MinimumVersionPolicyHandler::OnPolicyChanged() {
  ash::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&MinimumVersionPolicyHandler::OnPolicyChanged,
                         weak_factory_.GetWeakPtr()));
  if (status != ash::CrosSettingsProvider::TRUSTED || !IsPolicyApplicable() ||
      !ash::features::IsMinimumChromeVersionEnabled()) {
    VLOG(1) << "Ignore policy change - policy is not applicable or settings "
               "are not trusted.";
    return;
  }

  const base::Value::Dict* policy_value;
  if (!cros_settings_->GetDictionary(ash::kDeviceMinimumVersion,
                                     &policy_value)) {
    VLOG(1) << "Revoke policy - policy is unset or value is incorrect.";
    HandleUpdateNotRequired();
    return;
  }
  const base::Value::List* entries = policy_value->FindList(kRequirements);
  if (!entries || entries->empty()) {
    VLOG(1) << "Revoke policy - empty policy requirements.";
    HandleUpdateNotRequired();
    return;
  }
  auto restricted = policy_value->FindBool(kUnmanagedUserRestricted);
  unmanaged_user_restricted_ = restricted.value_or(false);

  std::vector<std::unique_ptr<MinimumVersionRequirement>> configs;
  for (const auto& item : *entries) {
    if (!item.is_dict())
      continue;
    std::unique_ptr<MinimumVersionRequirement> instance =
        MinimumVersionRequirement::CreateInstanceIfValid(item.GetDict());
    if (instance)
      configs.push_back(std::move(instance));
  }

  // Select the strongest config whose requirements are not satisfied by the
  // current version. The strongest config is chosen as the one whose minimum
  // required version is greater and closest to the current version. In case of
  // conflict. preference is given to the one with lesser warning time or eol
  // warning time.
  int strongest_config_idx = -1;
  std::vector<std::unique_ptr<MinimumVersionRequirement>>
      update_required_configs;
  for (unsigned int i = 0; i < configs.size(); i++) {
    MinimumVersionRequirement* item = configs[i].get();
    if (!CurrentVersionSatisfies(*item) &&
        (strongest_config_idx == -1 ||
         item->Compare(configs[strongest_config_idx].get()) < 0))
      strongest_config_idx = i;
  }

  if (strongest_config_idx != -1) {
    // Update is required if at least one config exists whose requirements are
    // not satisfied by the current version.
    std::unique_ptr<MinimumVersionRequirement> strongest_config =
        std::move(configs[strongest_config_idx]);
    if (!state_ || state_->Compare(strongest_config.get()) != 0) {
      state_ = std::move(strongest_config);
      FetchEolInfo();
    }
  } else {
    // Update is not required as the requirements of all of the configs in the
    // policy are satisfied by the current Chrome OS version. We could also
    // reach here at the time of login if the device was rebooted to apply the
    // downloaded update, in which case it is needed to reset the local state.
    HandleUpdateNotRequired();
  }
}

void MinimumVersionPolicyHandler::HandleUpdateNotRequired() {
  VLOG(2) << "Update is not required.";
  // Reset the state including any running timers.
  Reset();
  // Hide update required screen if it is visible and switch back to the login
  // screen.
  if (delegate_->IsLoginSessionState())
    delegate_->HideUpdateRequiredScreenIfShown();
  ResetRelaunchNotification();
}

void MinimumVersionPolicyHandler::Reset() {
  deadline_reached_ = false;
  eol_reached_ = false;
  update_required_deadline_ = base::Time();
  update_required_time_ = base::Time();
  update_required_deadline_timer_.Stop();
  notification_timer_.Stop();
  GetBuildState()->RemoveObserver(this);
  state_.reset();
  HideNotification();
  notification_handler_.reset();
  ResetLocalState();
  StopObservingNetwork();
}

void MinimumVersionPolicyHandler::ResetOnUpdateCompleted() {
  update_required_deadline_timer_.Stop();
  notification_timer_.Stop();
  GetBuildState()->RemoveObserver(this);
  HideNotification();
  notification_handler_.reset();
}

void MinimumVersionPolicyHandler::FetchEolInfo() {
  // Return if update required state is null meaning all requirements are
  // satisfied.
  if (!state_)
    return;

  update_required_time_ = clock_->Now();
  // Request the End of Life (Auto Update Expiration) status.
  UpdateEngineClient::Get()->GetEolInfo(
      base::BindOnce(&MinimumVersionPolicyHandler::OnFetchEolInfo,
                     weak_factory_.GetWeakPtr()));
}

void MinimumVersionPolicyHandler::OnFetchEolInfo(
    const UpdateEngineClient::EolInfo info) {
  if (!ash::switches::IsAueReachedForUpdateRequiredForTest() &&
      (info.eol_date.is_null() || info.eol_date > update_required_time_)) {
    // End of life is not reached. Start update with |warning_time_|.
    eol_reached_ = false;
    HandleUpdateRequired(state_->warning());
  } else {
    // End of life is reached. Start update with |eol_warning_time_|.
    eol_reached_ = true;
    HandleUpdateRequired(state_->eol_warning());
  }
  if (!fetch_eol_callback_.is_null())
    std::move(fetch_eol_callback_).Run();
}

void MinimumVersionPolicyHandler::HandleUpdateRequired(
    base::TimeDelta warning_time) {
  const base::Time stored_timer_start_time =
      local_state()->GetTime(prefs::kUpdateRequiredTimerStartTime);
  const base::TimeDelta stored_warning_time =
      local_state()->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod);
  base::Time previous_deadline = stored_timer_start_time + stored_warning_time;

  // If update is already required, use the existing timer start time to
  // calculate the new deadline. Else use |update_required_time_|. Do not reduce
  // the warning time if policy is already applied.
  if (stored_timer_start_time.is_null()) {
    update_required_deadline_ = update_required_time_ + warning_time;
  } else {
    update_required_deadline_ =
        stored_timer_start_time + std::max(stored_warning_time, warning_time);
  }
  VLOG(1) << "Update is required with "
          << "update required time " << update_required_time_
          << " warning time " << warning_time
          << " and update required deadline " << update_required_deadline_;

  const bool deadline_reached =
      update_required_deadline_ <= update_required_time_;
  if (deadline_reached) {
    // As per the policy, the deadline for the user cannot reduce.
    // This case can be encountered when :-
    // a) Update was not required before and now critical update is required.
    // b) Update was required and warning time has expired when device is
    // rebooted.
    // Update the local state with the current policy values.
    if (update_required_deadline_ > previous_deadline)
      UpdateLocalState(warning_time);
    OnDeadlineReached();
    return;
  }

  // Need to start the timer even if the deadline is same as the previous one to
  // handle the case of Chrome reboot.
  if (update_required_deadline_timer_.IsRunning() &&
      update_required_deadline_timer_.desired_run_time() ==
          update_required_deadline_) {
    DLOG(WARNING) << "Deadline is same as previous and timer is running.";
    return;
  }

  // This case can be encountered when :-
  // a) Update was not required before and now update is required with a
  // warning time. b) Policy has been updated with new values and update is
  // still required.

  // Hide update required screen if it is shown on the login screen.
  if (delegate_->IsLoginSessionState())
    delegate_->HideUpdateRequiredScreenIfShown();

  // The |deadline| can only be equal to or greater than the
  // |previous_deadline|. No need to update the local state if the deadline has
  // not been extended.
  if (update_required_deadline_ > previous_deadline)
    UpdateLocalState(warning_time);

  // The device has already downloaded the update in-session and waiting for
  // reboot to apply it.
  if (GetBuildState()->update_type() == BuildState::UpdateType::kNormalUpdate) {
    OverrideRelaunchNotification(update_required_deadline_);
    DLOG(WARNING) << "Update is already installed.";
    return;
  }

  StartDeadlineTimer(update_required_deadline_);
  if (!eol_reached_)
    StartObservingUpdate();
  ShowAndScheduleNotification(update_required_deadline_);
}

void MinimumVersionPolicyHandler::ResetLocalState() {
  local_state()->ClearPref(prefs::kUpdateRequiredTimerStartTime);
  local_state()->ClearPref(prefs::kUpdateRequiredWarningPeriod);
}

void MinimumVersionPolicyHandler::UpdateLocalState(
    base::TimeDelta warning_time) {
  base::Time timer_start_time =
      local_state()->GetTime(prefs::kUpdateRequiredTimerStartTime);
  if (timer_start_time.is_null()) {
    local_state()->SetTime(prefs::kUpdateRequiredTimerStartTime,
                           update_required_time_);
  }
  local_state()->SetTimeDelta(prefs::kUpdateRequiredWarningPeriod,
                              warning_time);
  local_state()->CommitPendingWrite();
}

void MinimumVersionPolicyHandler::StartDeadlineTimer(base::Time deadline) {
  // Start the timer to expire when deadline is reached and the device is not
  // updated to meet the policy requirements.
  update_required_deadline_timer_.Start(
      FROM_HERE, deadline,
      base::BindOnce(&MinimumVersionPolicyHandler::OnDeadlineReached,
                     weak_factory_.GetWeakPtr()));
}

void MinimumVersionPolicyHandler::StartObservingUpdate() {
  auto* build_state = GetBuildState();
  if (!build_state->HasObserver(this))
    build_state->AddObserver(this);
}

std::optional<int> MinimumVersionPolicyHandler::GetTimeRemainingInDays() {
  const base::Time now = clock_->Now();
  if (!state_ || update_required_deadline_ <= now)
    return std::nullopt;
  base::TimeDelta time_remaining = update_required_deadline_ - now;
  return GetDaysRounded(time_remaining);
}

void MinimumVersionPolicyHandler::MaybeShowNotificationOnLogin() {
  // |days| could be null if |update_required_deadline_timer_| expired while
  // login was in progress, else we would have shown the update required screen
  // at startup.
  std::optional<int> days = GetTimeRemainingInDays();
  if (days && days.value() <= 1)
    MaybeShowNotification(base::Days(days.value()));
}

void MinimumVersionPolicyHandler::MaybeShowNotification(
    base::TimeDelta warning) {
  const NetworkStatus status = GetCurrentNetworkStatus();
  if ((!eol_reached_ && status == NetworkStatus::kAllowed) ||
      !delegate_->IsUserLoggedIn() || !IsPolicyRestrictionAppliedForUser()) {
    return;
  }

  if (!notification_handler_) {
    notification_handler_ = std::make_unique<ash::UpdateRequiredNotification>();
  }

  NotificationType type = NotificationType::kNoConnection;
  base::OnceClosure button_click_callback;
  std::string manager = GetEnterpriseManager();
  std::u16string device_type = ui::GetChromeOSDeviceName();
  auto close_callback =
      base::BindOnce(&MinimumVersionPolicyHandler::StopObservingNetwork,
                     weak_factory_.GetWeakPtr());
  if (eol_reached_) {
    VLOG(2) << "Showing end of life notification.";
    type = NotificationType::kEolReached;
    button_click_callback = base::BindOnce(&OpenEnterpriseInfoPage);
  } else if (status == NetworkStatus::kMetered) {
    VLOG(2) << "Showing metered network notification.";
    type = NotificationType::kMeteredConnection;
    button_click_callback = base::BindOnce(
        &MinimumVersionPolicyHandler::UpdateOverMeteredPermssionGranted,
        weak_factory_.GetWeakPtr());
  } else if (status == NetworkStatus::kOffline) {
    VLOG(2) << "Showing no network notification.";
    button_click_callback = base::BindOnce(&OpenNetworkSettings);
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  notification_handler_->Show(type, warning, manager, device_type,
                              std::move(button_click_callback),
                              std::move(close_callback));

  if (!eol_reached_) {
    ash::NetworkStateHandler* network_state_handler =
        ash::NetworkHandler::Get()->network_state_handler();
    if (!network_state_handler->HasObserver(this))
      network_state_handler_observer_.Observe(network_state_handler);
  }
}

void MinimumVersionPolicyHandler::ShowAndScheduleNotification(
    base::Time deadline) {
  const base::Time now = clock_->Now();
  if (deadline <= now)
    return;

  base::Time expiry;
  base::TimeDelta time_remaining = deadline - now;
  int days_remaining = GetDaysRounded(time_remaining);

  // Network limitation notifications are shown when policy is received and on
  // the last day. End of life notifications are shown when policy is received,
  // one week before EOL and on the last day. No need to schedule a notification
  // if it is already the last day.
  if (eol_reached_ && days_remaining > kOneWeekEolNotificationInDays) {
    expiry = deadline - base::Days(kOneWeekEolNotificationInDays);
  } else if (days_remaining > 1) {
    expiry = deadline - base::Days(1);
  }

  VLOG(2) << "Next notification scheduled for " << expiry;
  MaybeShowNotification(base::Days(days_remaining));
  if (!expiry.is_null()) {
    notification_timer_.Start(
        FROM_HERE, expiry,
        base::BindOnce(
            &MinimumVersionPolicyHandler::ShowAndScheduleNotification,
            weak_factory_.GetWeakPtr(), deadline));
  }
}

void MinimumVersionPolicyHandler::OnUpdate(const BuildState* build_state) {
  // If the device has been successfully updated, the relaunch notifications
  // will reboot it for applying the updates.
  VLOG(1) << "Update installed successfully at " << clock_->Now()
          << " with deadline " << update_required_deadline_;
  UpdateEngineClient::Get()->RemoveObserver(this);
  if (build_state->update_type() == BuildState::UpdateType::kNormalUpdate) {
    ResetOnUpdateCompleted();
    OverrideRelaunchNotification(update_required_deadline_);
  }
}

void MinimumVersionPolicyHandler::HideNotification() const {
  if (notification_handler_)
    notification_handler_->Hide();
}

void MinimumVersionPolicyHandler::DefaultNetworkChanged(
    const ash::NetworkState* network) {
  // Close notification if network has switched to one that allows updates.
  const NetworkStatus status = GetCurrentNetworkStatus();
  if (status == NetworkStatus::kAllowed && notification_handler_) {
    HideNotification();
  }
}

void MinimumVersionPolicyHandler::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void MinimumVersionPolicyHandler::StopObservingNetwork() {
  network_state_handler_observer_.Reset();
}

void MinimumVersionPolicyHandler::UpdateOverMeteredPermssionGranted() {
  VLOG(1) << "Permission for update over metered network granted.";
  UpdateEngineClient* const update_engine_client = UpdateEngineClient::Get();
  if (!update_engine_client->HasObserver(this))
    update_engine_client->AddObserver(this);
  update_engine_client->RequestUpdateCheck(
      base::BindOnce(&MinimumVersionPolicyHandler::OnUpdateCheckStarted,
                     weak_factory_.GetWeakPtr()));
}

void MinimumVersionPolicyHandler::OnUpdateCheckStarted(
    UpdateEngineClient::UpdateCheckResult result) {
  VLOG(1) << "Update check started.";
  if (result != UpdateEngineClient::UPDATE_RESULT_SUCCESS)
    UpdateEngineClient::Get()->RemoveObserver(this);
}

void MinimumVersionPolicyHandler::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  if (status.current_operation() ==
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE) {
    UpdateEngineClient::Get()->SetUpdateOverCellularOneTimePermission(
        status.new_version(), status.new_size(),
        base::BindOnce(&MinimumVersionPolicyHandler::
                           OnSetUpdateOverCellularOneTimePermission,
                       weak_factory_.GetWeakPtr()));
  }
}

void MinimumVersionPolicyHandler::OnSetUpdateOverCellularOneTimePermission(
    bool success) {
  if (success)
    UpdateOverMeteredPermssionGranted();
  else
    UpdateEngineClient::Get()->RemoveObserver(this);
}

void MinimumVersionPolicyHandler::OnDeadlineReached() {
  deadline_reached_ = true;
  if (delegate_->IsLoginSessionState() && !delegate_->IsLoginInProgress()) {
    // Show update required screen over the login screen.
    delegate_->ShowUpdateRequiredScreen();
  } else if (delegate_->IsUserLoggedIn() &&
             IsPolicyRestrictionAppliedForUser()) {
    // Terminate the current user session to show update required
    // screen on the login screen if the user is managed or
    // |unmanaged_user_restricted_| is set to true.
    delegate_->RestartToLoginScreen();
  }
  // No action is required if -
  // 1) The user signed in is not managed. Once the un-managed user signs out or
  // the device is rebooted, the policy handler will be called again to show the
  // update required screen if required.
  // 2) Login is in progress. This would be handled in-session once user logs
  // in, the user would be logged out and update required screen is shown.
  // 3) Device has just been enrolled. The login screen would check and show the
  // update required screen.
}

}  // namespace policy
