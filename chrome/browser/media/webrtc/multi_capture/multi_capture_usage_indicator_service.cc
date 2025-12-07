// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/multi_capture_notification_details_view.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace multi_capture {
namespace {

constexpr size_t kAppLength = 18;

const char kPrivacyIndicatorsMultiCaptureLoginNotificationId[] =
    "multi-capture-login-privacy-indicators";
const char kPrivacyIndicatorsMultiCaptureLoginNotifierId[] =
    "multi-capture-login-privacy-indicators";
const char kPrivacyIndicatorsMultiCaptureNotificationIdPrefix[] =
    "multi-capture-active-privacy-indicators-";

constexpr auto kNotifierType = message_center::NotifierType::SYSTEM_COMPONENT;

std::string GenerateActiveNotifcationId(const webapps::AppId& app_id) {
  return kPrivacyIndicatorsMultiCaptureNotificationIdPrefix + app_id;
}

std::vector<std::string> GenerateAppNameList(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) {
  std::vector<std::string> app_names;
  base::Extend(app_names, apps.future_capture_notification_apps,
               [&](const auto& app) { return app.second; });
  base::Extend(app_names, apps.future_capture_no_notification_apps,
               [&](const auto& app) { return app.second; });
  std::sort(app_names.begin(), app_names.end());
  return app_names;
}

std::u16string CreateFutureCaptureNotificationMessage(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) {
  CHECK(!apps.future_capture_notification_apps.empty() ||
        !apps.future_capture_no_notification_apps.empty());

  std::u16string message;
  if (!apps.future_capture_notification_apps.empty() &&
      !apps.future_capture_no_notification_apps.empty()) {
    message = l10n_util::GetStringUTF16(
        IDS_MULTI_CAPTURE_MAY_CAPTURE_SOME_NOTIFY_NOTIFICATION_MESSAGE);
  } else if (!apps.future_capture_notification_apps.empty()) {
    message = l10n_util::GetStringUTF16(
        IDS_MULTI_CAPTURE_MAY_CAPTURE_ALL_NOTIFY_NOTIFICATION_MESSAGE);
  } else {
    message = l10n_util::GetStringUTF16(
        IDS_MULTI_CAPTURE_MAY_CAPTURE_NONE_NOTIFY_NOTIFICATION_MESSAGE);
  }

  const std::vector<std::string> merged_app_names = GenerateAppNameList(apps);
  if (merged_app_names.size() == 1) {
    return base::i18n::MessageFormatter::FormatWithNamedArgs(
        message, "NUM_APPS", /*plurality=*/1, "APP0_NAME",
        gfx::TruncateString(base::UTF8ToUTF16(merged_app_names[0]), kAppLength,
                            gfx::BreakType::WORD_BREAK));
  }
  const std::u16string app_name_0 =
      gfx::TruncateString(base::UTF8ToUTF16(merged_app_names[0]), kAppLength,
                          gfx::BreakType::WORD_BREAK);
  const std::u16string app_name_1 =
      gfx::TruncateString(base::UTF8ToUTF16(merged_app_names[1]), kAppLength,
                          gfx::BreakType::WORD_BREAK);
  return base::i18n::MessageFormatter::FormatWithNamedArgs(
      message, "NUM_APPS", static_cast<int>(merged_app_names.size()),
      "APP0_NAME", app_name_0, "APP1_NAME", app_name_1);
}

bool ShouldReuseFutureNotificationId(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) {
  return apps.future_capture_notification_apps.empty() &&
         apps.future_capture_no_notification_apps.empty();
}

}  // namespace

MultiCaptureUsageIndicatorService::AllowListedAppNames::AllowListedAppNames(
    std::map<webapps::AppId, std::string> future_capture_notification_apps,
    std::map<webapps::AppId, std::string> future_capture_no_notification_apps,
    std::map<webapps::AppId, std::string> current_capture_notification_apps)
    : future_capture_notification_apps(
          std::move(future_capture_notification_apps)),
      future_capture_no_notification_apps(
          std::move(future_capture_no_notification_apps)),
      current_capture_notification_apps(
          std::move(current_capture_notification_apps)) {}
MultiCaptureUsageIndicatorService::AllowListedAppNames::~AllowListedAppNames() =
    default;

MultiCaptureUsageIndicatorService::MultiCaptureUsageIndicatorService(
    Profile* profile,
    PrefService* prefs,
    MultiCaptureDataService* data_service)
    : pref_service_(prefs), data_service_(data_service), profile_(profile) {
  CHECK(pref_service_);
  CHECK(profile_);

  data_service_observer_.Observe(data_service_);
}

MultiCaptureUsageIndicatorService::~MultiCaptureUsageIndicatorService() =
    default;

std::unique_ptr<MultiCaptureUsageIndicatorService>
MultiCaptureUsageIndicatorService::Create(
    Profile* profile,
    PrefService* prefs,
    MultiCaptureDataService* data_service) {
  CHECK(prefs);
  CHECK(data_service);
  return base::WrapUnique(
      new MultiCaptureUsageIndicatorService(profile, prefs, data_service));
}

void MultiCaptureUsageIndicatorService::MultiCaptureStarted(
    const std::string& label,
    const webapps::AppId& app_id) {
  started_captures_[app_id].insert(label);
  label_to_app_id_[label] = app_id;
  RefreshNotifications();
}

// TODO(crbug.com/428895438): Trigger this function when the capturing window is
// closed.
void MultiCaptureUsageIndicatorService::MultiCaptureStopped(
    const std::string& label) {
  if (!label_to_app_id_.contains(label)) {
    return;
  }

  const webapps::AppId app_id = label_to_app_id_[label];
  started_captures_[app_id].erase(label);
  label_to_app_id_.erase(label);
  if (started_captures_[app_id].empty()) {
    notification_display_service().Close(NotificationHandler::Type::TRANSIENT,
                                         GenerateActiveNotifcationId(app_id));
    notification_shown_for_app_id_.erase(app_id);
    started_captures_.erase(app_id);
    RefreshNotifications();
  }
}

void MultiCaptureUsageIndicatorService::MultiCaptureDataChanged() {
  RefreshNotifications();
}

void MultiCaptureUsageIndicatorService::MultiCaptureDataServiceDestroyed() {
  data_service_observer_.Reset();
}

message_center::Notification
MultiCaptureUsageIndicatorService::CreateFutureCaptureNotification(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) {
  CHECK(!apps.future_capture_notification_apps.empty() ||
        !apps.future_capture_no_notification_apps.empty());

  message_center::RichNotificationData optional_fields;
  // Make the notification low priority so that it is silently added (no
  // popup).
  optional_fields.priority = message_center::LOW_PRIORITY;
  optional_fields.pinned = true;
  optional_fields.vector_small_image = &vector_icons::kScreenRecordIcon;

  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_BUTTON_TEXT));

  message_center::Notification notification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      // TODO(crbug.com/428895081): Prevent this text from being trimmed.
      kPrivacyIndicatorsMultiCaptureLoginNotificationId,
      /*title=*/CreateFutureCaptureNotificationMessage(apps),
      /*message=*/u"",
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(
          kNotifierType, kPrivacyIndicatorsMultiCaptureLoginNotifierId,
          ash::NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](base::WeakPtr<MultiCaptureUsageIndicatorService> service,
                 std::optional<int> button_index) {
                if (!service || !button_index) {
                  return;
                }

                const MultiCaptureUsageIndicatorService::AllowListedAppNames
                    app_names = service->GetInstalledAndAllowlistedAppNames();
                MultiCaptureNotificationDetailsView::ShowCaptureDetails(
                    service->GetAllCaptureWithNotificationApps(app_names),
                    service->GetAllCaptureWithoutNotificationApps(app_names));
              },
              weak_ptr_factory_.GetWeakPtr())));

  notification.set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::NORMAL);
  return notification;
}

message_center::Notification
MultiCaptureUsageIndicatorService::CreateActiveCaptureNotification(
    const webapps::AppId& app_id,
    const std::string& app_name,
    const bool should_reuse_future_notification_id) {
  message_center::RichNotificationData optional_fields;
  std::string notification_id;
  std::string notifier_id;
  if (should_reuse_future_notification_id) {
    notification_id = kPrivacyIndicatorsMultiCaptureLoginNotificationId;
    notifier_id = kPrivacyIndicatorsMultiCaptureLoginNotifierId;
  } else {
    notification_id = GenerateActiveNotifcationId(app_id);
    // Using this notifier ID will tie the notification to the privacy
    // indicators group and prevent a separate icon to show up in the system
    // tray.
    notifier_id = ash::kPrivacyIndicatorsMultiCaptureNotifierId;
  }

  if (notification_shown_for_app_id_.contains(app_id)) {
    // Make the notification low priority so that it is silently added (no
    // popup).
    optional_fields.priority = message_center::LOW_PRIORITY;
  }

  optional_fields.vector_small_image = &vector_icons::kScreenRecordIcon;
  optional_fields.pinned = true;

  message_center::Notification notification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      notification_id,
      /*title=*/
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_MULTI_CAPTURE_ACTIVE_CAPTURE_NOTIFICATION_MESSAGE),
          "NUM_APPS", /*plurality=*/1, "APP_NAME",
          gfx::TruncateString(base::UTF8ToUTF16(app_name), kAppLength,
                              gfx::BreakType::WORD_BREAK)),
      /*message=*/u"",
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(
          kNotifierType, notifier_id,
          ash::NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      // TODO(crbug.com/424104858): Make the notification do nothing on click.
      /*delegate=*/
      base::MakeRefCounted<message_center::NotificationDelegate>());
  notification.set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::NORMAL);

  return notification;
}

MultiCaptureUsageIndicatorService::AllowListedAppNames
MultiCaptureUsageIndicatorService::GetInstalledAndAllowlistedAppNames() const {
  CHECK(data_service_);

  const std::map<webapps::AppId, std::string>&
      future_capture_no_notification_apps =
          data_service_->GetCaptureAppsWithoutNotification();
  std::map<webapps::AppId, std::string> future_capture_notification_apps;
  std::map<webapps::AppId, std::string> current_capture_notification_apps;
  for (const auto& [app_id, app_name] :
       data_service_->GetCaptureAppsWithNotification()) {
    if (base::Contains(started_captures_, app_id)) {
      current_capture_notification_apps[app_id] = app_name;
    } else {
      future_capture_notification_apps[app_id] = app_name;
    }
  }

  return {std::move(future_capture_notification_apps),
          std::move(future_capture_no_notification_apps),
          std::move(current_capture_notification_apps)};
}

// TODO(crbug.com/424103935): Call again when a new app is installed that is
// already on the screen capture allowlist on session start.
void MultiCaptureUsageIndicatorService::ShowFutureMultiCaptureNotification(
    const AllowListedAppNames& apps) {
  if (apps.future_capture_notification_apps.empty() &&
      apps.future_capture_no_notification_apps.empty()) {
    return;
  }

  notification_display_service().Display(NotificationHandler::Type::TRANSIENT,
                                         CreateFutureCaptureNotification(apps),
                                         /*metadata=*/nullptr);
}

void MultiCaptureUsageIndicatorService::ShowActiveMultiCaptureNotifications(
    const AllowListedAppNames& apps) {
  CHECK(data_service_);

  std::vector<std::pair<webapps::AppId, std::string>> apps_to_notify;
  const std::map<webapps::AppId, std::string>& apps_with_notification =
      data_service_->GetCaptureAppsWithNotification();
  for (const auto& [app_id, _] : started_captures_) {
    if (!apps_with_notification.contains(app_id)) {
      continue;
    }

    apps_to_notify.emplace_back(app_id, apps_with_notification.at(app_id));
  }

  std::sort(apps_to_notify.begin(), apps_to_notify.end(),
            [](const auto& app_1, const auto& app_2) {
              return app_1.second < app_2.second;
            });

  bool reuse_future_notification_id = ShouldReuseFutureNotificationId(apps);
  for (const auto& [app_id, app_name] : apps_to_notify) {
    // If the notification shows "Your screen may be captured" and we want to
    // reuse that notification id, we need to close it first to make the
    // notification pop up.
    if (reuse_future_notification_id) {
      // TODO(crbug.com/428931746): Check that we only close an actual capturing
      // notification.
      notification_display_service().Close(
          NotificationHandler::Type::TRANSIENT,
          kPrivacyIndicatorsMultiCaptureLoginNotificationId);
    }

    // TODO(crbug.com/432202914): This works well, but in most cases we don't
    // actually need to close and reopen. Keep track of the already shown id's
    // and only execute this if there is a change.
    notification_display_service().Close(NotificationHandler::Type::TRANSIENT,
                                         GenerateActiveNotifcationId(app_id));
    notification_display_service().Display(
        NotificationHandler::Type::TRANSIENT,
        CreateActiveCaptureNotification(app_id, app_name,
                                        reuse_future_notification_id),
        /*metadata=*/nullptr);
    notification_shown_for_app_id_.insert(app_id);
    reuse_future_notification_id = false;
  }
}

void MultiCaptureUsageIndicatorService::RefreshNotifications() {
  const AllowListedAppNames apps = GetInstalledAndAllowlistedAppNames();
  if (apps.future_capture_notification_apps.empty() &&
      apps.future_capture_no_notification_apps.empty() &&
      apps.current_capture_notification_apps.empty()) {
    notification_display_service().Close(
        NotificationHandler::Type::TRANSIENT,
        kPrivacyIndicatorsMultiCaptureLoginNotificationId);
    return;
  }

  ShowFutureMultiCaptureNotification(apps);
  ShowActiveMultiCaptureNotifications(apps);
}

std::vector<MultiCaptureNotificationDetailsView::AppInfo>
MultiCaptureUsageIndicatorService::GetAllCaptureWithNotificationApps(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) const {
  std::vector<MultiCaptureNotificationDetailsView::AppInfo>
      capturing_apps_with_notification;
  for (const auto& [app_id, app_name] : apps.future_capture_notification_apps) {
    capturing_apps_with_notification.push_back(
        MultiCaptureNotificationDetailsView::AppInfo(
            app_name, data_service_->GetAppIcon(app_id)));
  }

  for (const auto& [app_id, app_name] :
       apps.current_capture_notification_apps) {
    capturing_apps_with_notification.push_back(
        MultiCaptureNotificationDetailsView::AppInfo(
            app_name, data_service_->GetAppIcon(app_id)));
  }

  std::sort(capturing_apps_with_notification.begin(),
            capturing_apps_with_notification.end(),
            [](const MultiCaptureNotificationDetailsView::AppInfo& app_1,
               const MultiCaptureNotificationDetailsView::AppInfo& app_2) {
              return app_1.name < app_2.name;
            });

  return capturing_apps_with_notification;
}

std::vector<MultiCaptureNotificationDetailsView::AppInfo>
MultiCaptureUsageIndicatorService::GetAllCaptureWithoutNotificationApps(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) const {
  std::vector<MultiCaptureNotificationDetailsView::AppInfo>
      capturing_apps_without_notification;
  for (const auto& [app_id, app_name] :
       apps.future_capture_no_notification_apps) {
    capturing_apps_without_notification.push_back(
        MultiCaptureNotificationDetailsView::AppInfo(
            app_name, data_service_->GetAppIcon(app_id)));
  }

  std::sort(capturing_apps_without_notification.begin(),
            capturing_apps_without_notification.end(),
            [](const MultiCaptureNotificationDetailsView::AppInfo& app_1,
               const MultiCaptureNotificationDetailsView::AppInfo& app_2) {
              return app_1.name < app_2.name;
            });

  return capturing_apps_without_notification;
}

NotificationDisplayService&
MultiCaptureUsageIndicatorService::notification_display_service() const {
  return CHECK_DEREF(
      NotificationDisplayServiceFactory::GetForProfile(profile_));
}

}  // namespace multi_capture
