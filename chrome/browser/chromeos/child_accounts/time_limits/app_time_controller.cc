// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_activity_provider.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace chromeos {
namespace app_time {

const char kAppsWithTimeLimitMetric[] =
    "SupervisedUsers.PerAppTimeLimits.AppsWithTimeLimit";
const char kBlockedAppsCountMetric[] =
    "SupervisedUsers.PerAppTimeLimits.BlockedAppsCount";
const char kPolicyChangeCountMetric[] =
    "SupervisedUsers.PerAppTimeLimits.PolicyChangeCount";
const char kEngagementMetric[] = "SupervisedUsers.PerAppTimeLimits.Engagement";

namespace {

constexpr base::TimeDelta kDay = base::TimeDelta::FromHours(24);

// Family link notifier id.
constexpr char kFamilyLinkSourceId[] = "family-link";

// Time limit reaching id. This id will be appended by the application name. The
// 5 minute and one minute notifications for the same app will have the same id.
constexpr char kAppTimeLimitReachingNotificationId[] =
    "time-limit-reaching-id-";

// Time limit updated id. This id will be appended by the application name.
constexpr char kAppTimeLimitUpdateNotificationId[] = "time-limit-updated-id-";

// Used to convert |time_limit| to string. |cutoff| specifies whether the
// formatted result will be one value or two values: for example if the time
// delta is 2 hours 30 minutes: |cutoff| of <2 will result in "2 hours" and
// |cutoff| of 3 will result in "2 hours and 30 minutes".
base::string16 GetTimeLimitMessage(base::TimeDelta time_limit, int cutoff) {
  return ui::TimeFormat::Detailed(ui::TimeFormat::Format::FORMAT_DURATION,
                                  ui::TimeFormat::Length::LENGTH_LONG, cutoff,
                                  time_limit);
}

base::string16 GetNotificationTitleFor(const base::string16& app_name,
                                       AppNotification notification) {
  switch (notification) {
    case AppNotification::kFiveMinutes:
    case AppNotification::kOneMinute:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_WILL_PAUSE_SYSTEM_NOTIFICATION_TITLE,
          app_name);
    case AppNotification::kBlocked:
    case AppNotification::kAvailable:
    case AppNotification::kTimeLimitChanged:
      return l10n_util::GetStringUTF16(
          IDS_APP_TIME_LIMIT_APP_TIME_LIMIT_SET_SYSTEM_NOTIFICATION_TITLE);
    default:
      NOTREACHED();
      return base::EmptyString16();
  }
}

base::string16 GetNotificationMessageFor(
    const base::string16& app_name,
    AppNotification notification,
    base::Optional<base::TimeDelta> time_limit) {
  switch (notification) {
    case AppNotification::kFiveMinutes:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_WILL_PAUSE_SYSTEM_NOTIFICATION_MESSAGE,
          GetTimeLimitMessage(base::TimeDelta::FromMinutes(5), /* cutoff */ 1));
    case AppNotification::kOneMinute:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_WILL_PAUSE_SYSTEM_NOTIFICATION_MESSAGE,
          GetTimeLimitMessage(base::TimeDelta::FromMinutes(1), /* cutoff */ 1));
    case AppNotification::kTimeLimitChanged:
      return time_limit
                 ? l10n_util::GetStringFUTF16(
                       IDS_APP_TIME_LIMIT_APP_TIME_LIMIT_SET_SYSTEM_NOTIFICATION_MESSAGE,
                       GetTimeLimitMessage(*time_limit, /* cutoff */ 3),
                       app_name)
                 : l10n_util::GetStringFUTF16(
                       IDS_APP_TIME_LIMIT_APP_TIME_LIMIT_REMOVED_SYSTEM_NOTIFICATION_MESSAGE,
                       app_name);
    case AppNotification::kBlocked:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_BLOCKED_NOTIFICATION_MESSAGE, app_name);

    case AppNotification::kAvailable:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_AVAILABLE_NOTIFICATION_MESSAGE, app_name);
    default:
      NOTREACHED();
      return base::EmptyString16();
  }
}

std::string GetNotificationIdFor(const std::string& app_name,
                                 AppNotification notification) {
  std::string notification_id;
  switch (notification) {
    case AppNotification::kFiveMinutes:
    case AppNotification::kOneMinute:
      notification_id = kAppTimeLimitReachingNotificationId;
      break;
    case AppNotification::kTimeLimitChanged:
    case AppNotification::kBlocked:
    case AppNotification::kAvailable:
      notification_id = kAppTimeLimitUpdateNotificationId;
      break;
    default:
      NOTREACHED();
      notification_id = "";
      break;
  }
  return base::StrCat({notification_id, app_name});
}

bool IsAppOpenedInChrome(const AppId& app_id, Profile* profile) {
  if (app_id.app_type() != apps::mojom::AppType::kExtension &&
      app_id.app_type() != apps::mojom::AppType::kWeb) {
    return false;
  }

  // It is a web or extension.
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id.app_id());
  if (!extension)
    return false;

  extensions::LaunchContainer launch_container = extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile), extension);
  return launch_container == extensions::LaunchContainer::kLaunchContainerTab;
}

}  // namespace

AppTimeController::TestApi::TestApi(AppTimeController* controller)
    : controller_(controller) {}

AppTimeController::TestApi::~TestApi() = default;

void AppTimeController::TestApi::SetLastResetTime(base::Time time) {
  controller_->SetLastResetTime(time);
}

base::Time AppTimeController::TestApi::GetNextResetTime() const {
  return controller_->GetNextResetTime();
}

base::Time AppTimeController::TestApi::GetLastResetTime() const {
  return controller_->last_limits_reset_time_;
}

AppActivityRegistry* AppTimeController::TestApi::app_registry() {
  return controller_->app_registry_.get();
}

// static
bool AppTimeController::ArePerAppTimeLimitsEnabled() {
  return base::FeatureList::IsEnabled(features::kPerAppTimeLimits);
}

bool AppTimeController::IsAppActivityReportingEnabled() {
  return base::FeatureList::IsEnabled(features::kAppActivityReporting);
}

// static
void AppTimeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kPerAppTimeLimitsLastResetTime, 0);
  registry->RegisterDictionaryPref(prefs::kPerAppTimeLimitsPolicy);
  registry->RegisterDictionaryPref(prefs::kPerAppTimeLimitsAllowlistPolicy);
}

AppTimeController::AppTimeController(Profile* profile)
    : profile_(profile),
      app_service_wrapper_(std::make_unique<AppServiceWrapper>(profile)),
      app_registry_(
          std::make_unique<AppActivityRegistry>(app_service_wrapper_.get(),
                                                this,
                                                profile->GetPrefs())),
      web_time_activity_provider_(std::make_unique<WebTimeActivityProvider>(
          this,
          app_service_wrapper_.get())) {
  DCHECK(profile);

  if (WebTimeLimitEnforcer::IsEnabled())
    web_time_enforcer_ = std::make_unique<WebTimeLimitEnforcer>(this);

  PrefService* pref_service = profile->GetPrefs();
  RegisterProfilePrefObservers(pref_service);
  TimeLimitsAllowlistPolicyUpdated(prefs::kPerAppTimeLimitsAllowlistPolicy);
  TimeLimitsPolicyUpdated(prefs::kPerAppTimeLimitsPolicy);

  // Restore the last reset time. If reset time has have been crossed, triggers
  // AppActivityRegistry to clear up the running active times of applications.
  RestoreLastResetTime();

  // Start observing system clock client and time zone settings.
  auto* system_clock_client = SystemClockClient::Get();
  // SystemClockClient may not be initialized in some tests.
  if (system_clock_client)
    system_clock_client->AddObserver(this);

  auto* time_zone_settings = system::TimezoneSettings::GetInstance();
  if (time_zone_settings)
    time_zone_settings->AddObserver(this);

  // Start observing |app_registry_|
  app_registry_->AddAppStateObserver(this);

  // AppActivityRegistry may have missed |OnAppInstalled| calls. Notify it.
  app_registry_->SetInstalledApps(app_service_wrapper_->GetInstalledApps());

  // Record enagement metrics.
  base::UmaHistogramCounts1000(kEngagementMetric, apps_with_limit_);

  // If chrome is paused at the beginning of the session, notify
  // web_time_enforcer directly. This is a workaround for bug in AppService that
  // occurs at the beginning of the session. It could be removed when
  // AppService successfully calls OnWebTimeLimitReached at the beginning of
  // sessions.
  if (app_registry_->IsAppInstalled(GetChromeAppId()) &&
      app_registry_->IsAppTimeLimitReached(GetChromeAppId())) {
    base::Optional<AppLimit> web_time_limit = app_registry_->GetWebTimeLimit();
    DCHECK(web_time_limit);
    DCHECK(web_time_limit->daily_limit());
    DCHECK(web_time_enforcer_);
    web_time_enforcer_->OnWebTimeLimitReached(
        web_time_limit->daily_limit().value());
  }
}

AppTimeController::~AppTimeController() {
  app_registry_->RemoveAppStateObserver(this);

  auto* time_zone_settings = system::TimezoneSettings::GetInstance();
  if (time_zone_settings)
    time_zone_settings->RemoveObserver(this);

  auto* system_clock_client = SystemClockClient::Get();
  if (system_clock_client)
    system_clock_client->RemoveObserver(this);
}

bool AppTimeController::IsExtensionAllowlisted(
    const std::string& extension_id) const {
  return true;
}

base::Optional<base::TimeDelta> AppTimeController::GetTimeLimitForApp(
    const std::string& app_service_id,
    apps::mojom::AppType app_type) const {
  const app_time::AppId app_id =
      app_service_wrapper_->AppIdFromAppServiceId(app_service_id, app_type);
  return app_registry_->GetTimeLimit(app_id);
}

void AppTimeController::RecordMetricsOnShutdown() const {
  base::UmaHistogramCounts1000(kPolicyChangeCountMetric,
                               patl_policy_update_count_);
}

void AppTimeController::SystemClockUpdated() {
  if (HasTimeCrossedResetBoundary())
    OnResetTimeReached();
}

void AppTimeController::TimezoneChanged(const icu::TimeZone& timezone) {
  // Timezone changes may not require us to reset information,
  // however, they may require updating the scheduled reset time.
  ScheduleForTimeLimitReset();
}

void AppTimeController::RegisterProfilePrefObservers(
    PrefService* pref_service) {
  pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_registrar_->Init(pref_service);

  // Adds callbacks to observe policy pref changes.
  // Using base::Unretained(this) is safe here because when |pref_registrar_|
  // gets destroyed, it will remove the observers from PrefService.
  pref_registrar_->Add(
      prefs::kPerAppTimeLimitsPolicy,
      base::BindRepeating(&AppTimeController::TimeLimitsPolicyUpdated,
                          base::Unretained(this)));
  pref_registrar_->Add(
      prefs::kPerAppTimeLimitsAllowlistPolicy,
      base::BindRepeating(&AppTimeController::TimeLimitsAllowlistPolicyUpdated,
                          base::Unretained(this)));
}

void AppTimeController::TimeLimitsPolicyUpdated(const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kPerAppTimeLimitsPolicy);

  const base::Value* policy =
      pref_registrar_->prefs()->GetDictionary(prefs::kPerAppTimeLimitsPolicy);

  if (!policy || !policy->is_dict()) {
    LOG(WARNING) << "Invalid PerAppTimeLimits policy.";
    return;
  }
  std::map<AppId, AppLimit> app_limits = policy::AppLimitsFromDict(*policy);

  // If web time limit feature is not enabled, then remove chrome's time limit
  // from here.
  if (!WebTimeLimitEnforcer::IsEnabled() &&
      base::Contains(app_limits, GetChromeAppId())) {
    app_limits.erase(GetChromeAppId());
  }

  bool updated = app_registry_->UpdateAppLimits(app_limits);

  app_registry_->SetReportingEnabled(
      policy::ActivityReportingEnabledFromDict(*policy));

  base::Optional<base::TimeDelta> new_reset_time =
      policy::ResetTimeFromDict(*policy);
  // TODO(agawronska): Propagate the information about reset time change.
  if (new_reset_time && *new_reset_time != limits_reset_time_)
    limits_reset_time_ = *new_reset_time;

  apps_with_limit_ =
      app_registry_->GetAppsWithAppRestriction(AppRestriction::kTimeLimit)
          .size();

  if (updated) {
    patl_policy_update_count_++;

    base::UmaHistogramCounts1000(kAppsWithTimeLimitMetric, apps_with_limit_);

    int blocked_apps =
        app_registry_->GetAppsWithAppRestriction(AppRestriction::kBlocked)
            .size();

    base::UmaHistogramCounts1000(kBlockedAppsCountMetric, blocked_apps);
  }
}

void AppTimeController::TimeLimitsAllowlistPolicyUpdated(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kPerAppTimeLimitsAllowlistPolicy);

  const base::DictionaryValue* policy = pref_registrar_->prefs()->GetDictionary(
      prefs::kPerAppTimeLimitsAllowlistPolicy);

  // Figure out a way to avoid cloning
  AppTimeLimitsAllowlistPolicyWrapper wrapper(policy);

  app_registry_->OnTimeLimitAllowlistChanged(wrapper);

  if (web_time_enforcer_)
    web_time_enforcer_->OnTimeLimitAllowlistChanged(wrapper);
}

void AppTimeController::ShowAppTimeLimitNotification(
    const AppId& app_id,
    const base::Optional<base::TimeDelta>& time_limit,
    AppNotification notification) {
  DCHECK_NE(AppNotification::kUnknown, notification);

  if (notification == AppNotification::kTimeLimitReached)
    return;

  const std::string app_name = app_service_wrapper_->GetAppName(app_id);
  int size_hint_in_dp = 48;
  app_service_wrapper_->GetAppIcon(
      app_id, size_hint_in_dp,
      base::BindOnce(&AppTimeController::ShowNotificationForApp,
                     weak_ptr_factory_.GetWeakPtr(), app_name, notification,
                     time_limit));
}

void AppTimeController::OnAppLimitReached(const AppId& app_id,
                                          base::TimeDelta time_limit,
                                          bool was_active) {
  bool show_dialog = was_active;
  if (app_id == GetChromeAppId() || IsAppOpenedInChrome(app_id, profile_))
    show_dialog = false;

  app_service_wrapper_->PauseApp(PauseAppInfo(app_id, time_limit, show_dialog));

  // TODO(crbug/1074516) This is a temporary workaround. The underlying problem
  // should be fixed.
  if (app_id == GetChromeAppId() && web_time_enforcer_)
    web_time_enforcer_->OnWebTimeLimitReached(time_limit);
}

void AppTimeController::OnAppLimitRemoved(const AppId& app_id) {
  app_service_wrapper_->ResumeApp(app_id);

  // TODO(crbug/1074516) This is a temporary workaround. The underlying problem
  // should be fixed.
  if (app_id == GetChromeAppId() && web_time_enforcer_)
    web_time_enforcer_->OnWebTimeLimitEnded();
}

void AppTimeController::OnAppInstalled(const AppId& app_id) {
  if (!WebTimeLimitEnforcer::IsEnabled() && IsWebAppOrExtension(app_id))
    return;

  const base::Value* allowlist_policy = pref_registrar_->prefs()->GetDictionary(
      prefs::kPerAppTimeLimitsAllowlistPolicy);
  if (allowlist_policy && allowlist_policy->is_dict()) {
    AppTimeLimitsAllowlistPolicyWrapper wrapper(allowlist_policy);
    if (base::Contains(wrapper.GetAllowlistAppList(), app_id))
      app_registry_->SetAppAllowlisted(app_id);
  } else {
    LOG(WARNING) << " Invalid PerAppTimeLimitAllowlist policy";
  }

  const base::Value* policy =
      pref_registrar_->prefs()->GetDictionary(prefs::kPerAppTimeLimitsPolicy);

  if (!policy || !policy->is_dict()) {
    LOG(WARNING) << "Invalid PerAppTimeLimits policy.";
    return;
  }

  // Update the application's time limit.
  const std::map<AppId, AppLimit> limits = policy::AppLimitsFromDict(*policy);
  // Update the limit for newly installed app, if it exists.
  auto result = limits.find(app_id);
  if (result == limits.end())
    return;

  app_registry_->SetAppLimit(result->first, result->second);
}

base::Time AppTimeController::GetNextResetTime() const {
  // UTC time now.
  base::Time now = base::Time::Now();

  // UTC time local midnight.
  base::Time nearest_midnight = now.LocalMidnight();

  base::Time prev_midnight;
  if (now > nearest_midnight)
    prev_midnight = nearest_midnight;
  else
    prev_midnight = nearest_midnight - base::TimeDelta::FromHours(24);

  base::Time next_reset_time = prev_midnight + limits_reset_time_;

  if (next_reset_time > now)
    return next_reset_time;

  // We have already reset for this day. The reset time is the next day.
  return next_reset_time + base::TimeDelta::FromHours(24);
}

void AppTimeController::ScheduleForTimeLimitReset() {
  if (reset_timer_.IsRunning())
    reset_timer_.AbandonAndStop();

  base::TimeDelta time_until_reset = GetNextResetTime() - base::Time::Now();
  reset_timer_.Start(FROM_HERE, time_until_reset,
                     base::BindOnce(&AppTimeController::OnResetTimeReached,
                                    base::Unretained(this)));
}

void AppTimeController::OnResetTimeReached() {
  base::Time now = base::Time::Now();

  app_registry_->OnResetTimeReached(now);

  SetLastResetTime(now);

  ScheduleForTimeLimitReset();
}

void AppTimeController::RestoreLastResetTime() {
  PrefService* pref_service = profile_->GetPrefs();
  int64_t reset_time =
      pref_service->GetInt64(prefs::kPerAppTimeLimitsLastResetTime);

  if (reset_time == 0) {
    SetLastResetTime(base::Time::Now());
  } else {
    last_limits_reset_time_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(reset_time));
  }

  if (HasTimeCrossedResetBoundary()) {
    OnResetTimeReached();
  } else {
    ScheduleForTimeLimitReset();
  }
}

void AppTimeController::SetLastResetTime(base::Time timestamp) {
  // |timestamp| needs to be adjusted to ensure that it is happening at the time
  // specified by policy.
  const base::Time nearest_midnight = timestamp.LocalMidnight();
  base::Time prev_midnight;
  if (timestamp > nearest_midnight)
    prev_midnight = nearest_midnight;
  else
    prev_midnight = nearest_midnight - base::TimeDelta::FromHours(24);

  base::Time reset_time = prev_midnight + limits_reset_time_;
  if (reset_time <= timestamp)
    last_limits_reset_time_ = reset_time;
  else
    last_limits_reset_time_ = reset_time - base::TimeDelta::FromHours(24);

  PrefService* service = profile_->GetPrefs();
  DCHECK(service);
  service->SetInt64(
      prefs::kPerAppTimeLimitsLastResetTime,
      last_limits_reset_time_.ToDeltaSinceWindowsEpoch().InMicroseconds());
  service->CommitPendingWrite();
}

bool AppTimeController::HasTimeCrossedResetBoundary() const {
  // Time after system time or timezone changed.
  base::Time now = base::Time::Now();

  return now < last_limits_reset_time_ || now >= kDay + last_limits_reset_time_;
}

void AppTimeController::OpenFamilyLinkApp() {
  const std::string app_id = arc::ArcPackageNameToAppId(
      chromeos::ChildUserService::kFamilyLinkHelperAppPackageName, profile_);

  if (app_service_wrapper_->IsAppInstalled(app_id)) {
    // Launch Family Link Help app since it is available.
    app_service_wrapper_->LaunchApp(app_id);
    return;
  }
  // No Family Link Help app installed, so try to launch Play Store to Family
  // Link Help app install page.
  arc::LaunchPlayStoreWithUrl(
      chromeos::ChildUserService::kFamilyLinkHelperAppPlayStoreURL);
}

void AppTimeController::ShowNotificationForApp(
    const std::string& app_name,
    AppNotification notification,
    base::Optional<base::TimeDelta> time_limit,
    base::Optional<gfx::ImageSkia> icon) {
  DCHECK(notification == AppNotification::kFiveMinutes ||
         notification == AppNotification::kOneMinute ||
         notification == AppNotification::kTimeLimitChanged ||
         notification == AppNotification::kBlocked ||
         notification == AppNotification::kAvailable);

  DCHECK(notification == AppNotification::kTimeLimitChanged ||
         notification == AppNotification::kBlocked ||
         notification == AppNotification::kAvailable || time_limit.has_value());

  // Alright we have all the messages that we want.
  const base::string16 app_name_16 = base::UTF8ToUTF16(app_name);
  const base::string16 title =
      GetNotificationTitleFor(app_name_16, notification);
  const base::string16 message =
      GetNotificationMessageFor(app_name_16, notification, time_limit);
  // Family link display source.
  const base::string16 notification_source =
      l10n_util::GetStringUTF16(IDS_TIME_LIMIT_NOTIFICATION_DISPLAY_SOURCE);

  std::string notification_id = GetNotificationIdFor(app_name, notification);
  message_center::RichNotificationData option_fields;
  option_fields.fullscreen_visibility =
      message_center::FullscreenVisibility::OVER_USER;

  std::unique_ptr<message_center::Notification> message_center_notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
          message, notification_source, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kFamilyLinkSourceId),
          option_fields,
          notification == AppNotification::kTimeLimitChanged
              ? base::MakeRefCounted<
                    message_center::HandleNotificationClickDelegate>(
                    base::BindRepeating(&AppTimeController::OpenFamilyLinkApp,
                                        weak_ptr_factory_.GetWeakPtr()))
              : base::MakeRefCounted<message_center::NotificationDelegate>(),
          chromeos::kNotificationSupervisedUserIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  if (icon.has_value())
    message_center_notification->set_icon(gfx::Image(icon.value()));

  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  if (!notification_display_service)
    return;

  // Close the existing notification with notification_id.
  notification_display_service->Close(NotificationHandler::Type::TRANSIENT,
                                      notification_id);

  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *message_center_notification,
                                        /*metadata=*/nullptr);
}

}  // namespace app_time
}  // namespace chromeos
