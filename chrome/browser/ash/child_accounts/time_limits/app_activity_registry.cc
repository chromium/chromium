// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_notification_delegate.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/ash/child_accounts/time_limits/persisted_app_info.h"
#include "chrome/common/pref_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace ash {
namespace app_time {

namespace {

constexpr base::TimeDelta kFiveMinutes = base::Minutes(5);
constexpr base::TimeDelta kOneMinute = base::Minutes(1);
constexpr base::TimeDelta kZeroMinutes = base::Minutes(0);

enterprise_management::AppActivity::AppState AppStateForReporting(
    AppState state) {
  switch (state) {
    case AppState::kAvailable:
      return enterprise_management::AppActivity::DEFAULT;
    case AppState::kAlwaysAvailable:
      return enterprise_management::AppActivity::ALWAYS_AVAILABLE;
    case AppState::kBlocked:
      return enterprise_management::AppActivity::BLOCKED;
    case AppState::kLimitReached:
      return enterprise_management::AppActivity::LIMIT_REACHED;
    case AppState::kUninstalled:
      return enterprise_management::AppActivity::UNINSTALLED;
    default:
      return enterprise_management::AppActivity::UNKNOWN;
  }
}

}  // namespace

AppActivityRegistry::TestApi::TestApi(AppActivityRegistry* registry)
    : registry_(registry) {}

AppActivityRegistry::TestApi::~TestApi() = default;

const std::optional<AppLimit>& AppActivityRegistry::TestApi::GetAppLimit(
    const AppId& app_id) const {
  DCHECK(base::Contains(registry_->activity_registry_, app_id));
  return registry_->activity_registry_.at(app_id).limit;
}

std::optional<base::TimeDelta> AppActivityRegistry::TestApi::GetTimeLeft(
    const AppId& app_id) const {
  return registry_->GetTimeLeftForApp(app_id);
}

void AppActivityRegistry::TestApi::SaveAppActivity() {
  registry_->SaveAppActivity();
}

AppActivityRegistry::SystemNotification::SystemNotification(
    std::optional<base::TimeDelta> app_time_limit,
    AppNotification app_notification)
    : time_limit(app_time_limit), notification(app_notification) {}

AppActivityRegistry::SystemNotification::SystemNotification(
    const SystemNotification&) = default;

AppActivityRegistry::SystemNotification&
AppActivityRegistry::SystemNotification::operator=(const SystemNotification&) =
    default;

AppActivityRegistry::AppDetails::AppDetails() = default;

AppActivityRegistry::AppDetails::AppDetails(const AppActivity& activity)
    : activity(activity) {}

AppActivityRegistry::AppDetails::~AppDetails() = default;

void AppActivityRegistry::AppDetails::ResetTimeCheck() {
  activity.set_last_notification(AppNotification::kUnknown);
  if (app_limit_timer)
    app_limit_timer->AbandonAndStop();
}

bool AppActivityRegistry::AppDetails::IsLimitReached() const {
  if (!limit.has_value())
    return false;

  if (limit->restriction() != AppRestriction::kTimeLimit)
    return false;

  DCHECK(limit->daily_limit());
  if (limit->daily_limit() > activity.RunningActiveTime())
    return false;

  return true;
}

bool AppActivityRegistry::AppDetails::IsLimitEqual(
    const std::optional<AppLimit>& another_limit) const {
  if (limit.has_value() != another_limit.has_value())
    return false;

  if (!limit.has_value())
    return true;

  if (limit->restriction() == another_limit->restriction() &&
      limit->daily_limit() == another_limit->daily_limit()) {
    return true;
  }

  return false;
}

// static
void AppActivityRegistry::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kPerAppTimeLimitsAppActivities);
  registry->RegisterInt64Pref(prefs::kPerAppTimeLimitsLastSuccessfulReportTime,
                              0);
  registry->RegisterInt64Pref(prefs::kPerAppTimeLimitsLatestLimitUpdateTime, 0);
}

AppActivityRegistry::AppActivityRegistry(
    AppServiceWrapper* app_service_wrapper,
    AppTimeNotificationDelegate* notification_delegate,
    PrefService* pref_service)
    : pref_service_(pref_service),
      app_service_wrapper_(app_service_wrapper),
      notification_delegate_(notification_delegate),
      save_data_to_pref_service_(base::DefaultTickClock::GetInstance()) {
  DCHECK(app_service_wrapper_);
  DCHECK(notification_delegate_);
  DCHECK(pref_service_);

  if (ShouldCleanUpStoredPref())
    CleanRegistry(base::Time::Now() - base::Days(30));

  InitializeRegistryFromPref();

  save_data_to_pref_service_.Start(FROM_HERE, base::Minutes(5), this,
                                   &AppActivityRegistry::SaveAppActivity);

  app_service_wrapper_->AddObserver(this);
}

AppActivityRegistry::~AppActivityRegistry() {
  app_service_wrapper_->RemoveObserver(this);
}

void AppActivityRegistry::OnAppInstalled(const AppId& app_id) {
  // App might be already present in registry, because we preserve info between
  // sessions and app service does not. Make sure not to override cached state.
  if (!base::Contains(activity_registry_, app_id)) {
    Add(app_id);
  } else {
    activity_registry_.at(app_id).received_app_installed_ = true;

    // First send the system notifications for the application.
    SendSystemNotificationsForApp(app_id);

    if (GetAppState(app_id) == AppState::kLimitReached) {
      NotifyLimitReached(app_id, /* was_active */ false);
    } else if (GetAppState(app_id) == AppState::kUninstalled) {
      OnAppReinstalled(app_id);
    }
  }
}

void AppActivityRegistry::OnAppUninstalled(const AppId& app_id) {
  // TODO(agawronska): Consider DCHECK instead of it. Not sure if there are
  // legit cases when we might go out of sync with AppService.
  if (base::Contains(activity_registry_, app_id))
    SetAppState(app_id, AppState::kUninstalled);
}

void AppActivityRegistry::OnAppAvailable(const AppId& app_id) {
  if (!base::Contains(activity_registry_, app_id))
    return;

  AppState prev_state = GetAppState(app_id);

  if (prev_state == AppState::kLimitReached)
    return;

  // This may happen in the scenario where the application is uninstalled and
  // reinstalled in the same session.
  if (prev_state == AppState::kUninstalled) {
    OnAppReinstalled(app_id);
  }

  if (IsWebAppOrExtension(app_id) && app_id != GetChromeAppId() &&
      base::Contains(activity_registry_, GetChromeAppId()) &&
      GetAppState(app_id) == AppState::kBlocked) {
    SetAppState(app_id, GetAppState(GetChromeAppId()));
    return;
  }

  SetAppState(app_id, AppState::kAvailable);
}

void AppActivityRegistry::OnAppBlocked(const AppId& app_id) {
  if (!base::Contains(activity_registry_, app_id))
    return;

  if (GetAppState(app_id) == AppState::kBlocked)
    return;

  SetAppState(app_id, AppState::kBlocked);
}

void AppActivityRegistry::OnAppActive(const AppId& app_id,
                                      const base::UnguessableToken& instance_id,
                                      base::Time timestamp) {
  if (!base::Contains(activity_registry_, app_id))
    return;

  if (app_id == GetChromeAppId())
    return;

  AppDetails& app_details = activity_registry_[app_id];

  // We are notified that a paused app is active. Notify observers to pause it.
  if (GetAppState(app_id) == AppState::kLimitReached) {
    // If the instance is in |app_details.paused_instances| then
    // AppActivityRegistry has already notified its observers to pause it.
    // Return.
    if (base::Contains(app_details.paused_instances, instance_id))
      return;

    app_details.paused_instances.insert(instance_id);
    NotifyLimitReached(app_id, /* was_active */ true);
    return;
  }

  if (!IsAppAvailable(app_id))
    return;

  std::set<base::UnguessableToken>& active_instances =
      app_details.active_instances;

  if (base::Contains(active_instances, instance_id))
    return;

  active_instances.insert(instance_id);

  // No need to set app as active if there were already active instances for the
  // app
  if (active_instances.size() > 1)
    return;

  SetAppActive(app_id, timestamp);
}

void AppActivityRegistry::OnAppInactive(
    const AppId& app_id,
    const base::UnguessableToken& instance_id,
    base::Time timestamp) {
  if (!base::Contains(activity_registry_, app_id))
    return;

  if (app_id == GetChromeAppId())
    return;

  std::set<base::UnguessableToken>& active_instances =
      activity_registry_[app_id].active_instances;

  if (!base::Contains(active_instances, instance_id))
    return;

  active_instances.erase(instance_id);
  if (active_instances.size() > 0)
    return;

  SetAppInactive(app_id, timestamp);
}

void AppActivityRegistry::OnAppDestroyed(
    const AppId& app_id,
    const base::UnguessableToken& instance_id,
    base::Time timestamp) {
  if (!base::Contains(activity_registry_, app_id))
    return;

  if (app_id == GetChromeAppId())
    return;

  AppDetails& app_details = activity_registry_.at(app_id);
  if (base::Contains(app_details.paused_instances, instance_id))
    app_details.paused_instances.erase(instance_id);
}

bool AppActivityRegistry::IsAppInstalled(const AppId& app_id) const {
  if (base::Contains(activity_registry_, app_id))
    return GetAppState(app_id) != AppState::kUninstalled;
  return false;
}

bool AppActivityRegistry::IsAppAvailable(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  auto state = GetAppState(app_id);
  return state == AppState::kAvailable || state == AppState::kAlwaysAvailable;
}

bool AppActivityRegistry::IsAppBlocked(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return GetAppState(app_id) == AppState::kBlocked;
}

bool AppActivityRegistry::IsAppTimeLimitReached(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return GetAppState(app_id) == AppState::kLimitReached;
}

bool AppActivityRegistry::IsAppActive(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return activity_registry_.at(app_id).activity.is_active();
}

bool AppActivityRegistry::IsAllowlistedApp(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return GetAppState(app_id) == AppState::kAlwaysAvailable;
}

void AppActivityRegistry::AddAppStateObserver(
    AppActivityRegistry::AppStateObserver* observer) {
  app_state_observers_.AddObserver(observer);
}

void AppActivityRegistry::RemoveAppStateObserver(
    AppActivityRegistry::AppStateObserver* observer) {
  app_state_observers_.RemoveObserver(observer);
}

void AppActivityRegistry::SetInstalledApps(
    const std::vector<AppId>& installed_apps) {
  for (const auto& app : installed_apps)
    OnAppInstalled(app);
}

base::TimeDelta AppActivityRegistry::GetActiveTime(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return activity_registry_.at(app_id).activity.RunningActiveTime();
}

const std::optional<AppLimit>& AppActivityRegistry::GetWebTimeLimit() const {
  DCHECK(base::Contains(activity_registry_, GetChromeAppId()));
  return activity_registry_.at(GetChromeAppId()).limit;
}

AppState AppActivityRegistry::GetAppState(const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  return activity_registry_.at(app_id).activity.app_state();
}

std::optional<base::TimeDelta> AppActivityRegistry::GetTimeLimit(
    const AppId& app_id) const {
  if (!base::Contains(activity_registry_, app_id))
    return std::nullopt;

  const std::optional<AppLimit>& limit = activity_registry_.at(app_id).limit;
  if (!limit || limit->restriction() != AppRestriction::kTimeLimit)
    return std::nullopt;

  DCHECK(limit->daily_limit());
  return limit->daily_limit();
}

void AppActivityRegistry::SetReportingEnabled(std::optional<bool> value) {
  if (value.has_value())
    activity_reporting_enabled_ = value.value();
}

void AppActivityRegistry::GenerateHiddenApps(
    enterprise_management::ChildStatusReportRequest* report) {
  const std::vector<AppId> hidden_arc_apps =
      app_service_wrapper_->GetHiddenArcApps();
  for (const auto& app_id : hidden_arc_apps) {
    enterprise_management::App* app_info = report->add_hidden_app();
    app_info->set_app_id(app_id.app_id());
    app_info->set_app_type(AppTypeForReporting(app_id.app_type()));
    if (app_id.app_type() == apps::AppType::kArc) {
      app_info->add_additional_app_id(
          app_service_wrapper_->GetAppServiceId(app_id));
    }
  }
}

AppActivityReportInterface::ReportParams
AppActivityRegistry::GenerateAppActivityReport(
    enterprise_management::ChildStatusReportRequest* report) {
  // Calling SaveAppActivity is beneficial even if this method is returning
  // early due to reporting not being enabled. This is because it helps move the
  // ActiveTimes information from AppActivityRegistry to the stored pref data
  // which will then be cleaned in the direct CleanRegistry() call below.
  SaveAppActivity();

  // If app activity reporting is not enabled, simply return.
  if (!activity_reporting_enabled_) {
    base::Time timestamp = base::Time::Now();
    CleanRegistry(timestamp);
    return AppActivityReportInterface::ReportParams{timestamp, false};
  }

  const base::Value::List& list =
      pref_service_->GetList(prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> applications_info =
      PersistedAppInfo::PersistedAppInfosFromList(
          list,
          /* include_app_activity_array */ true);

  const base::Time timestamp = base::Time::Now();
  bool anything_reported = false;

  for (const auto& entry : applications_info) {
    const AppId& app_id = entry.app_id();
    const std::vector<AppActivity::ActiveTime>& active_times =
        entry.active_times();

    // Do not report if there is no activity.
    if (active_times.empty())
      continue;

    enterprise_management::AppActivity* app_activity =
        report->add_app_activity();
    enterprise_management::App* app_info = app_activity->mutable_app_info();
    app_info->set_app_id(app_id.app_id());
    app_info->set_app_type(AppTypeForReporting(app_id.app_type()));
    // AppService is is only different for ARC++ apps.
    if (app_id.app_type() == apps::AppType::kArc) {
      app_info->add_additional_app_id(
          app_service_wrapper_->GetAppServiceId(app_id));
    }
    app_activity->set_app_state(AppStateForReporting(entry.app_state()));
    app_activity->set_populated_at(timestamp.InMillisecondsSinceUnixEpoch());

    for (const auto& active_time : active_times) {
      enterprise_management::TimePeriod* time_period =
          app_activity->add_active_time_periods();
      time_period->set_start_timestamp(
          active_time.active_from().InMillisecondsSinceUnixEpoch());
      time_period->set_end_timestamp(
          active_time.active_to().InMillisecondsSinceUnixEpoch());
    }
    anything_reported = true;
  }

  return AppActivityReportInterface::ReportParams{timestamp, anything_reported};
}

void AppActivityRegistry::OnSuccessfullyReported(base::Time timestamp) {
  CleanRegistry(timestamp);

  // Update last successful report time.
  pref_service_->SetInt64(
      prefs::kPerAppTimeLimitsLastSuccessfulReportTime,
      timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

bool AppActivityRegistry::UpdateAppLimits(
    const std::map<AppId, AppLimit>& app_limits) {
  base::Time latest_update = latest_app_limit_update_;
  bool policy_updated = false;
  for (auto& entry : activity_registry_) {
    const AppId& app_id = entry.first;

    // Web time limits are updated when chrome's time limit is updated.
    if (app_id != GetChromeAppId() && IsWebAppOrExtension(app_id))
      continue;

    std::optional<AppLimit> new_limit = std::nullopt;
    if (base::Contains(app_limits, app_id))
      new_limit = app_limits.at(app_id);

    policy_updated |= SetAppLimit(app_id, new_limit);

    if (new_limit && new_limit->last_updated() > latest_update)
      latest_update = new_limit->last_updated();
  }

  latest_app_limit_update_ = latest_update;

  // Update the latest app limit update.
  pref_service_->SetInt64(
      prefs::kPerAppTimeLimitsLatestLimitUpdateTime,
      latest_app_limit_update_.ToDeltaSinceWindowsEpoch().InMicroseconds());

  return policy_updated;
}

bool AppActivityRegistry::SetAppLimit(
    const AppId& app_id,
    const std::optional<AppLimit>& app_limit) {
  DCHECK(base::Contains(activity_registry_, app_id));

  // If an application is not installed but present in the registry return
  // early.
  if (!IsAppInstalled(app_id))
    return false;

  // Chrome and web apps should not be blocked.
  if (app_limit && app_limit->restriction() == AppRestriction::kBlocked &&
      IsWebAppOrExtension(app_id)) {
    return false;
  }

  AppDetails& details = activity_registry_.at(app_id);
  // Limit 'data' are considered equal if only the |last_updated_| is different.
  // Update the limit to store new |last_updated_| value.
  bool did_change = !details.IsLimitEqual(app_limit);
  bool updated =
      ShowLimitUpdatedNotificationIfNeeded(app_id, details.limit, app_limit);
  details.limit = app_limit;

  // If |did_change| is false, handle the following corner case before
  // returning. The default value for app limit during construction at the
  // beginning of the session is std::nullopt. If the application was paused in
  // the previous session, and its limit was removed or feature is disabled in
  // the current session, the |app_limit| provided will be std::nullopt. Since
  // both values(the default app limit and the |app_limit| provided as an
  // argument for this method) are the same std::nullopt, |did_change| will be
  // false. But we still need to update the state to available as the new app
  // limit is std::nullopt.
  if (!did_change && (IsAppAvailable(app_id) || app_limit.has_value()))
    return updated;

  if (IsAllowlistedApp(app_id)) {
    if (app_limit.has_value()) {
      VLOG(1) << "Tried to set time limit for " << app_id
              << " which is allowlisted.";
    }

    details.limit = std::nullopt;
    return false;
  }

  if (!IsWebAppOrExtension(app_id)) {
    AppLimitUpdated(app_id);
    return updated;
  }

  for (auto& entry : activity_registry_) {
    const AppId& app_id_for_entry = entry.first;
    AppDetails& details_for_entry = entry.second;
    if (ContributesToWebTimeLimit(app_id_for_entry,
                                  GetAppState(app_id_for_entry))) {
      details_for_entry.limit = app_limit;
    }
  }

  for (auto& entry : activity_registry_) {
    const AppId& app_id_for_entry = entry.first;
    if (ContributesToWebTimeLimit(app_id_for_entry,
                                  GetAppState(app_id_for_entry))) {
      AppLimitUpdated(app_id_for_entry);
    }
  }

  return updated;
}

void AppActivityRegistry::SetAppAllowlisted(const AppId& app_id) {
  if (!base::Contains(activity_registry_, app_id))
    return;
  SetAppState(app_id, AppState::kAlwaysAvailable);
}

void AppActivityRegistry::OnChromeAppActivityChanged(
    ChromeAppActivityState state,
    base::Time timestamp) {
  AppId chrome_app_id = GetChromeAppId();
  if (!base::Contains(activity_registry_, chrome_app_id))
    return;

  AppDetails& details = activity_registry_[chrome_app_id];
  bool was_active = details.activity.is_active();

  bool is_active = (state == ChromeAppActivityState::kActive);

  // No need to notify observers that limit has reached. They will be notified
  // in AppActivityRegistry::OnAppActive.
  if (GetAppState(chrome_app_id) == AppState::kLimitReached && is_active)
    return;

  // No change in state.
  if (was_active == is_active)
    return;

  if (is_active) {
    SetAppActive(chrome_app_id, timestamp);
    return;
  }

  SetAppInactive(chrome_app_id, timestamp);
}

void AppActivityRegistry::OnTimeLimitAllowlistChanged(
    const AppTimeLimitsAllowlistPolicyWrapper& wrapper) {
  std::vector<AppId> allowlisted_apps = wrapper.GetAllowlistAppList();
  for (const AppId& app : allowlisted_apps) {
    if (!base::Contains(activity_registry_, app))
      continue;

    if (GetAppState(app) == AppState::kAlwaysAvailable)
      continue;

    std::optional<AppLimit>& limit = activity_registry_.at(app).limit;
    if (limit.has_value())
      limit = std::nullopt;

    SetAppState(app, AppState::kAlwaysAvailable);
  }
}

void AppActivityRegistry::SaveAppActivity() {
  {
    ScopedListPrefUpdate update(pref_service_,
                                prefs::kPerAppTimeLimitsAppActivities);
    base::Value::List& list = update.Get();

    const base::Time now = base::Time::Now();

    for (base::Value& entry : list) {
      std::optional<AppId> app_id =
          policy::AppIdFromAppInfoDict(entry.GetIfDict());
      DCHECK(app_id.has_value());

      if (!base::Contains(activity_registry_, app_id.value())) {
        std::optional<AppState> state =
            PersistedAppInfo::GetAppStateFromDict(entry.GetIfDict());
        DCHECK(state.has_value() && state.value() == AppState::kUninstalled);
        continue;
      }

      const PersistedAppInfo info =
          GetPersistedAppInfoForApp(app_id.value(), now);
      info.UpdateAppActivityPreference(entry.GetDict(), /* replace */ false);
    }

    for (const AppId& app_id : newly_installed_apps_) {
      const PersistedAppInfo info = GetPersistedAppInfoForApp(app_id, now);
      base::Value::Dict value;
      info.UpdateAppActivityPreference(value, /* replace */ false);
      list.Append(std::move(value));
    }
    newly_installed_apps_.clear();
  }

  // Ensure that the app activity is persisted.
  pref_service_->CommitPendingWrite();
}

std::vector<AppId> AppActivityRegistry::GetAppsWithAppRestriction(
    AppRestriction restriction) const {
  std::vector<AppId> apps_with_limit;
  for (const auto& entry : activity_registry_) {
    const AppId& app = entry.first;
    const AppDetails& details = entry.second;
    if (details.limit && details.limit->restriction() == restriction) {
      apps_with_limit.push_back(app);
    }
  }
  return apps_with_limit;
}

void AppActivityRegistry::OnResetTimeReached(base::Time timestamp) {
  for (std::pair<const AppId, AppDetails>& info : activity_registry_) {
    const AppId& app = info.first;
    AppDetails& details = info.second;

    // Reset running active time.
    details.activity.ResetRunningActiveTime(timestamp);

    // If timer is running, stop timer. Abandon all tasks set.
    details.ResetTimeCheck();

    // If the time limit has been reached, mark the app as available.
    if (details.activity.app_state() == AppState::kLimitReached)
      SetAppState(app, AppState::kAvailable);

    // If the application is currently active, schedule a time limit
    // check.
    if (details.activity.is_active())
      ScheduleTimeLimitCheckForApp(app);
  }
}

void AppActivityRegistry::CleanRegistry(base::Time timestamp) {
  ScopedListPrefUpdate update(pref_service_,
                              prefs::kPerAppTimeLimitsAppActivities);

  base::Value::List& list = update.Get();

  for (size_t index = 0; index < list.size();) {
    base::Value& entry = list[index];
    std::optional<PersistedAppInfo> info =
        PersistedAppInfo::PersistedAppInfoFromDict(entry.GetIfDict(), true);
    DCHECK(info.has_value());
    info->RemoveActiveTimeEarlierThan(timestamp);
    info->UpdateAppActivityPreference(entry.GetDict(), /* replace */ true);

    if (info->ShouldRemoveApp()) {
      // Remove entry in |activity_registry_| if it is present.
      activity_registry_.erase(info->app_id());

      // To efficiently remove the entry, swap it with the last element and pop
      // back.
      if (index < list.size() - 1)
        std::swap(list[index], list[list.size() - 1]);
      list.erase(list.end() - 1);
    } else {
      ++index;
    }
  }
}

void AppActivityRegistry::OnAppReinstalled(const AppId& app_id) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& details = activity_registry_.at(app_id);
  if (details.IsLimitReached()) {
    SetAppState(app_id, AppState::kLimitReached);
  } else {
    SetAppState(app_id, AppState::kAvailable);
  }

  // Notify observers.
  for (auto& observer : app_state_observers_)
    observer.OnAppInstalled(app_id);
}

void AppActivityRegistry::Add(const AppId& app_id) {
  activity_registry_[app_id].activity = AppActivity(AppState::kAvailable);
  activity_registry_[app_id].received_app_installed_ = true;

  bool is_app_chrome = app_id == GetChromeAppId();
  bool is_web = IsWebAppOrExtension(app_id);
  bool is_chrome_installed =
      base::Contains(activity_registry_, GetChromeAppId());
  if (!is_app_chrome && is_web && is_chrome_installed) {
    activity_registry_[app_id].limit = GetWebTimeLimit();
    activity_registry_[app_id].activity.SetAppState(
        GetAppState(GetChromeAppId()));
  }

  newly_installed_apps_.push_back(app_id);
  for (auto& observer : app_state_observers_)
    observer.OnAppInstalled(app_id);
}

void AppActivityRegistry::SetAppState(const AppId& app_id, AppState app_state) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& app_details = activity_registry_.at(app_id);
  AppActivity& app_activity = app_details.activity;
  AppState previous_state = app_activity.app_state();

  // There was no change in state, return.
  if (previous_state == app_state)
    return;

  app_activity.SetAppState(app_state);

  if (app_activity.app_state() == AppState::kLimitReached) {
    bool was_active = false;
    if (app_activity.is_active()) {
      was_active = true;
      app_details.paused_instances = std::move(app_details.active_instances);
      SetAppInactive(app_id, base::Time::Now());
    }

    NotifyLimitReached(app_id, was_active);
    return;
  }

  if (previous_state == AppState::kLimitReached &&
      app_activity.app_state() != AppState::kLimitReached) {
    for (auto& observer : app_state_observers_)
      observer.OnAppLimitRemoved(app_id);
    return;
  }
}

void AppActivityRegistry::NotifyLimitReached(const AppId& app_id,
                                             bool was_active) {
  DCHECK(base::Contains(activity_registry_, app_id));
  DCHECK_EQ(GetAppState(app_id), AppState::kLimitReached);

  const std::optional<AppLimit>& limit = activity_registry_.at(app_id).limit;
  DCHECK(limit->daily_limit());
  for (auto& observer : app_state_observers_) {
    observer.OnAppLimitReached(app_id, limit->daily_limit().value(),
                               was_active);
  }
}

void AppActivityRegistry::SetAppActive(const AppId& app_id,
                                       base::Time timestamp) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& app_details = activity_registry_[app_id];
  DCHECK(!app_details.activity.is_active());
  if (ContributesToWebTimeLimit(app_id, GetAppState(app_id)))
    app_details.activity.set_running_active_time(GetWebActiveRunningTime());

  app_details.activity.SetAppActive(timestamp);

  ScheduleTimeLimitCheckForApp(app_id);
}

void AppActivityRegistry::SetAppInactive(const AppId& app_id,
                                         base::Time timestamp) {
  DCHECK(base::Contains(activity_registry_, app_id));
  auto& details = activity_registry_.at(app_id);

  details.activity.SetAppInactive(timestamp);
  details.ResetTimeCheck();

  // If the application is a web app, synchronize its running active time with
  // those of other inactive web apps.
  if (ContributesToWebTimeLimit(app_id, GetAppState(app_id))) {
    base::TimeDelta active_time = details.activity.RunningActiveTime();
    for (auto& app_info : activity_registry_) {
      const AppId& app_id_for_info = app_info.first;
      if (!ContributesToWebTimeLimit(app_id_for_info,
                                     GetAppState(app_id_for_info))) {
        continue;
      }

      AppDetails& details_for_info = app_info.second;
      if (!details_for_info.activity.is_active())
        details_for_info.activity.set_running_active_time(active_time);
    }
  }
}

void AppActivityRegistry::ScheduleTimeLimitCheckForApp(const AppId& app_id) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& app_details = activity_registry_[app_id];

  // If there is no time limit information, don't set the timer.
  if (!app_details.limit.has_value())
    return;

  const AppLimit& limit = app_details.limit.value();
  if (limit.restriction() != AppRestriction::kTimeLimit)
    return;

  if (!app_details.app_limit_timer) {
    app_details.app_limit_timer = std::make_unique<base::OneShotTimer>(
        base::DefaultTickClock::GetInstance());
  }

  DCHECK(!app_details.app_limit_timer->IsRunning());

  // Check that the timer instance has been created.
  std::optional<base::TimeDelta> time_limit = GetTimeLeftForApp(app_id);
  DCHECK(time_limit.has_value());

  if (time_limit > kFiveMinutes) {
    time_limit = time_limit.value() - kFiveMinutes;
  } else if (time_limit > kOneMinute) {
    time_limit = time_limit.value() - kOneMinute;
  } else if (time_limit == kZeroMinutes) {
    // Zero minutes case could be handled by using the timer below, but we call
    // it explicitly to simplify tests.
    CheckTimeLimitForApp(app_id);
    return;
  }

  VLOG(1) << "Schedule app time limit check for " << app_id << " for "
          << time_limit.value();

  app_details.app_limit_timer->Start(
      FROM_HERE, time_limit.value(),
      base::BindOnce(&AppActivityRegistry::CheckTimeLimitForApp,
                     base::Unretained(this), app_id));
}

std::optional<base::TimeDelta> AppActivityRegistry::GetTimeLeftForApp(
    const AppId& app_id) const {
  DCHECK(base::Contains(activity_registry_, app_id));
  const AppDetails& app_details = activity_registry_.at(app_id);

  // If |app_details.limit| doesn't have value, the app has no restriction.
  if (!app_details.limit.has_value())
    return std::nullopt;

  const AppLimit& limit = app_details.limit.value();

  if (limit.restriction() != AppRestriction::kTimeLimit)
    return std::nullopt;

  // If the app has kTimeLimit restriction, DCHECK that daily limit has value.
  DCHECK(limit.daily_limit().has_value());

  AppState state = app_details.activity.app_state();
  if (state == AppState::kAlwaysAvailable || state == AppState::kBlocked)
    return std::nullopt;

  if (state == AppState::kLimitReached)
    return kZeroMinutes;

  DCHECK(state == AppState::kAvailable);

  base::TimeDelta time_limit = limit.daily_limit().value();

  base::TimeDelta active_time;
  if (ContributesToWebTimeLimit(app_id, GetAppState(app_id))) {
    active_time = GetWebActiveRunningTime();
  } else {
    active_time = app_details.activity.RunningActiveTime();
  }

  if (active_time >= time_limit)
    return kZeroMinutes;

  return time_limit - active_time;
}

void AppActivityRegistry::CheckTimeLimitForApp(const AppId& app_id) {
  AppDetails& details = activity_registry_[app_id];

  std::optional<base::TimeDelta> time_left = GetTimeLeftForApp(app_id);
  AppNotification last_notification = details.activity.last_notification();

  if (!time_left.has_value())
    return;

  DCHECK(details.limit.has_value());
  DCHECK(details.limit->daily_limit().has_value());
  const base::TimeDelta time_limit = details.limit->daily_limit().value();

  if (time_left <= kFiveMinutes && time_left > kOneMinute &&
      last_notification != AppNotification::kFiveMinutes) {
    MaybeShowSystemNotification(
        app_id, SystemNotification(time_limit, AppNotification::kFiveMinutes));
    ScheduleTimeLimitCheckForApp(app_id);
    return;
  }

  if (time_left <= kOneMinute && time_left > kZeroMinutes &&
      last_notification != AppNotification::kOneMinute) {
    MaybeShowSystemNotification(
        app_id, SystemNotification(time_limit, AppNotification::kOneMinute));
    ScheduleTimeLimitCheckForApp(app_id);
    return;
  }

  if (time_left == kZeroMinutes &&
      last_notification != AppNotification::kTimeLimitReached) {
    MaybeShowSystemNotification(
        app_id,
        SystemNotification(time_limit, AppNotification::kTimeLimitReached));

    if (ContributesToWebTimeLimit(app_id, GetAppState(app_id))) {
      WebTimeLimitReached(base::Time::Now());
    } else {
      SetAppState(app_id, AppState::kLimitReached);
    }
  }
}

bool AppActivityRegistry::ShowLimitUpdatedNotificationIfNeeded(
    const AppId& app_id,
    const std::optional<AppLimit>& old_limit,
    const std::optional<AppLimit>& new_limit) {
  // Web app limit changes are covered by Chrome notification.
  if (app_id != GetChromeAppId() && IsWebAppOrExtension(app_id))
    return false;

  // Don't show notification if the time limit's update was older than the
  // latest update.
  if (new_limit && new_limit->last_updated() <= latest_app_limit_update_)
    return false;

  const bool was_blocked =
      old_limit && old_limit->restriction() == AppRestriction::kBlocked;
  const bool is_blocked =
      new_limit && new_limit->restriction() == AppRestriction::kBlocked;

  if (!was_blocked && is_blocked) {
    MaybeShowSystemNotification(
        app_id, SystemNotification(std::nullopt, AppNotification::kBlocked));
    return true;
  }

  const bool had_time_limit =
      old_limit && old_limit->restriction() == AppRestriction::kTimeLimit;
  const bool has_time_limit =
      new_limit && new_limit->restriction() == AppRestriction::kTimeLimit;

  if (was_blocked && !is_blocked && !has_time_limit) {
    MaybeShowSystemNotification(
        app_id, SystemNotification(std::nullopt, AppNotification::kAvailable));
    return true;
  }

  // Time limit was removed.
  if (!has_time_limit && had_time_limit) {
    MaybeShowSystemNotification(
        app_id,
        SystemNotification(std::nullopt, AppNotification::kTimeLimitChanged));
    return true;
  }

  // Time limit was set or value changed.
  if (has_time_limit && (!had_time_limit || old_limit->daily_limit() !=
                                                new_limit->daily_limit())) {
    MaybeShowSystemNotification(
        app_id, SystemNotification(new_limit->daily_limit(),
                                   AppNotification::kTimeLimitChanged));
    return true;
  }

  return false;
}

base::TimeDelta AppActivityRegistry::GetWebActiveRunningTime() const {
  base::TimeDelta active_running_time = base::Seconds(0);
  for (const auto& app_info : activity_registry_) {
    const AppId& app_id = app_info.first;
    const AppDetails& details = app_info.second;
    if (!ContributesToWebTimeLimit(app_id, GetAppState(app_id))) {
      continue;
    }

    active_running_time = details.activity.RunningActiveTime();

    // If the app is active, then it has the most up to date active running
    // time.
    if (details.activity.is_active())
      return active_running_time;
  }

  return active_running_time;
}

void AppActivityRegistry::WebTimeLimitReached(base::Time timestamp) {
  for (auto& app_info : activity_registry_) {
    const AppId& app_id = app_info.first;
    if (!ContributesToWebTimeLimit(app_id, GetAppState(app_id)))
      continue;

    SetAppState(app_id, AppState::kLimitReached);
  }
}

void AppActivityRegistry::InitializeRegistryFromPref() {
  DCHECK(pref_service_);

  int64_t last_limits_updates =
      pref_service_->GetInt64(prefs::kPerAppTimeLimitsLatestLimitUpdateTime);

  latest_app_limit_update_ = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(last_limits_updates));

  InitializeAppActivities();
}

void AppActivityRegistry::InitializeAppActivities() {
  const base::Value::List& list =
      pref_service_->GetList(prefs::kPerAppTimeLimitsAppActivities);

  const std::vector<PersistedAppInfo> applications_info =
      PersistedAppInfo::PersistedAppInfosFromList(
          list,
          /* include_app_activity_array */ false);

  for (const auto& app_info : applications_info) {
    DCHECK(!base::Contains(activity_registry_, app_info.app_id()));

    // Don't restore uninstalled application's if its running active time is
    // zero.
    if (!app_info.ShouldRestoreApp())
      continue;

    activity_registry_[app_info.app_id()].activity =
        AppActivity(app_info.app_state(), app_info.active_running_time());
  }
}

PersistedAppInfo AppActivityRegistry::GetPersistedAppInfoForApp(
    const AppId& app_id,
    base::Time timestamp) {
  DCHECK(base::Contains(activity_registry_, app_id));

  AppDetails& details = activity_registry_.at(app_id);

  base::TimeDelta running_active_time = details.activity.RunningActiveTime();
  if (ContributesToWebTimeLimit(app_id, GetAppState(app_id)))
    running_active_time = GetWebActiveRunningTime();

  // Updates |AppActivity::active_times_| to include the current activity up to
  // |timestamp|.
  details.activity.CaptureOngoingActivity(timestamp);

  std::vector<AppActivity::ActiveTime> activity =
      details.activity.TakeActiveTimes();

  // If reporting is not enabled, don't save unnecessary data.
  if (!activity_reporting_enabled_)
    activity.clear();

  return PersistedAppInfo(app_id, details.activity.app_state(),
                          running_active_time, std::move(activity));
}

bool AppActivityRegistry::ShouldCleanUpStoredPref() {
  int64_t last_time =
      pref_service_->GetInt64(prefs::kPerAppTimeLimitsLastSuccessfulReportTime);

  if (last_time == 0)
    return false;

  base::Time time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(last_time));

  return time < base::Time::Now() - base::Days(30);
}

void AppActivityRegistry::SendSystemNotificationsForApp(const AppId& app_id) {
  DCHECK(base::Contains(activity_registry_, app_id));

  AppDetails& app_details = activity_registry_.at(app_id);
  DCHECK(app_details.received_app_installed_);

  // TODO(yilkal): Filter out the notifications to show. For example don't show
  // 5 min and 1 min left notifications at the same time here. However, time
  // limit changed and 1 min left notifications can be shown at the same time.
  for (const auto& elem : app_details.pending_notifications_) {
    notification_delegate_->ShowAppTimeLimitNotification(
        app_id, elem.time_limit, elem.notification);
  }
  app_details.pending_notifications_.clear();
}

void AppActivityRegistry::MaybeShowSystemNotification(
    const AppId& app_id,
    const SystemNotification& notification) {
  DCHECK(base::Contains(activity_registry_, app_id));

  AppDetails& app_details = activity_registry_.at(app_id);
  app_details.activity.set_last_notification(notification.notification);

  // AppActivityRegistry has not yet received OnAppInstalled call from
  // AppService. Add notification to |AppDetails::pending_notifications_|.
  if (!app_details.received_app_installed_) {
    app_details.pending_notifications_.push_back(notification);
    return;
  }

  // Otherwise, just show the notification.
  notification_delegate_->ShowAppTimeLimitNotification(
      app_id, notification.time_limit, notification.notification);
}

void AppActivityRegistry::AppLimitUpdated(const AppId& app_id) {
  DCHECK(base::Contains(activity_registry_, app_id));
  AppDetails& details = activity_registry_.at(app_id);

  // Limit for the active app changed - adjust the timers.
  // Handling of active app is different, because ongoing activity needs to be
  // taken into account.
  if (IsAppActive(app_id)) {
    details.ResetTimeCheck();
    ScheduleTimeLimitCheckForApp(app_id);
    return;
  }

  // Inactive available app reached the limit - update the state.
  // If app is in any other state than |kAvailable| the limit does not take
  // effect.
  if (IsAppAvailable(app_id) && details.IsLimitReached()) {
    SetAppInactive(app_id, base::Time::Now());
    SetAppState(app_id, AppState::kLimitReached);
    return;
  }

  // Paused inactive app is below the limit again - update the state.
  // This can happen if the limit was removed or new limit is greater the the
  // previous one. We know that the state should be available, because app can
  // only reach the limit if it is available.
  if (IsAppTimeLimitReached(app_id) && !details.IsLimitReached())
    SetAppState(app_id, AppState::kAvailable);
}

}  // namespace app_time
}  // namespace ash
