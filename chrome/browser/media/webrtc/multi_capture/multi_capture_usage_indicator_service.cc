// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/message_formatter.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace multi_capture {
namespace {

constexpr size_t kAppLength = 18;

const char kPrivacyIndicatorsMultiCaptureLoginNotificationId[] =
    "multi-capture-login-privacy-indicators";
const char kPrivacyIndicatorsMultiCaptureLoginNotifierId[] =
    "multi-capture-privacy-indicators";
const char kPrivacyIndicatorsMultiCaptureActiveNotificationIdBase[] =
    "multi-capture-active-privacy-indicators-";

std::string GenerateActiveNotifcationId(const webapps::AppId& app_id) {
  return kPrivacyIndicatorsMultiCaptureActiveNotificationIdBase + app_id;
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

message_center::Notification CreateFutureCaptureNotification(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) {
  CHECK(!apps.future_capture_notification_apps.empty() ||
        !apps.future_capture_no_notification_apps.empty());

  message_center::RichNotificationData optional_fields;
  // Make the notification low priority so that it is silently added (no
  // popup).
  optional_fields.priority = message_center::LOW_PRIORITY;
  optional_fields.pinned = true;
  // TODO(crbug.com/424102053): Replace with finalized icon.
  optional_fields.vector_small_image = &vector_icons::kScreenShareIcon;

  message_center::Notification notification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      // TODO(crbug.com/428895081): Prevent this text from being trimmed.
      kPrivacyIndicatorsMultiCaptureLoginNotificationId,
      /*title=*/u"",
      /*message=*/
      CreateFutureCaptureNotificationMessage(apps),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          kPrivacyIndicatorsMultiCaptureLoginNotifierId,
          ash::NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      // TODO(crbug.com/424104858): Add button to show more details on the
      // capturing apps.
      /*delegate=*/nullptr);
  notification.set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification.set_accent_color_id(ui::kColorAshPrivacyIndicatorsBackground);

  return notification;
}

message_center::Notification CreateActiveCaptureNotification(
    const webapps::AppId& app_id,
    const std::string& app_name,
    bool reuse_future_notification_id) {
  std::string notificiation_id;
  std::string notifier_id;
  if (reuse_future_notification_id) {
    notificiation_id = kPrivacyIndicatorsMultiCaptureLoginNotificationId;
    notifier_id = kPrivacyIndicatorsMultiCaptureLoginNotifierId;
  } else {
    notificiation_id = GenerateActiveNotifcationId(app_id);
    // Using this notifier ID will tie the notification to the privacy
    // indicators group and prevent a separate icon to show up in the system
    // tray.
    notifier_id = ash::kPrivacyIndicatorsNotifierId;
  }

  message_center::RichNotificationData optional_fields;
  // TODO(crbug.com/424102053): Replace with finalized icon.
  optional_fields.vector_small_image = &vector_icons::kScreenShareIcon;
  optional_fields.pinned = true;

  message_center::Notification notification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      notificiation_id,
      /*title=*/u"",
      /*message=*/
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_MULTI_CAPTURE_ACTIVE_CAPTURE_NOTIFICATION_MESSAGE),
          "NUM_APPS", /*plurality=*/1, "APP_NAME",
          gfx::TruncateString(base::UTF8ToUTF16(app_name), kAppLength,
                              gfx::BreakType::WORD_BREAK)),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, notifier_id,
          ash::NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      // TODO(crbug.com/424104858): Make the notification do nothing on click.
      /*delegate=*/nullptr);
  notification.set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification.set_accent_color_id(ui::kColorAshPrivacyIndicatorsBackground);

  return notification;
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
    PrefService* prefs,
    web_app::WebAppProvider* provider,
    NotificationDisplayService* notification_display_service)
    : pref_service_(prefs),
      provider_(provider),
      notification_display_service_(notification_display_service) {
  CHECK(pref_service_);
  CHECK(provider_);
  CHECK(notification_display_service_);
}

MultiCaptureUsageIndicatorService::~MultiCaptureUsageIndicatorService() =
    default;

std::unique_ptr<MultiCaptureUsageIndicatorService>
MultiCaptureUsageIndicatorService::Create(
    PrefService* prefs,
    web_app::WebAppProvider* provider,
    NotificationDisplayService* notification_display_service) {
  auto service = base::WrapUnique(new MultiCaptureUsageIndicatorService(
      prefs, provider, notification_display_service));
  service->ShowUsageIndicatorsOnStart();
  return service;
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
    notification_display_service_->Close(
        NotificationHandler::Type::ANNOUNCEMENT,
        GenerateActiveNotifcationId(app_id));
    started_captures_.erase(app_id);
    RefreshNotifications();
  }
}

void MultiCaptureUsageIndicatorService::ShowUsageIndicatorsOnStart() {
  // Fetch the initial value of the multi screen capture allowlist for later
  // matching to prevent dynamic refresh. We intentionally break dynamic
  // refresh as it is not possible to add further screen capture apps after
  // session start due to privacy constraints.
  multi_screen_capture_allow_list_on_login_ =
      pref_service_
          ->GetList(capture_policy::kManagedMultiScreenCaptureAllowedForUrls)
          .Clone();

  web_app::IwaKeyDistributionInfoProvider& key_distribution_info_provider =
      web_app::IwaKeyDistributionInfoProvider::GetInstance();
  if (provider_->is_registry_ready() &&
      key_distribution_info_provider.OnMaybeDownloadedComponentDataReady()
          .is_signaled()) {
    RefreshNotifications();
    return;
  }

  auto initialized_components_barrier = base::BarrierClosure(
      2u,
      base::BindOnce(&MultiCaptureUsageIndicatorService::RefreshNotifications,
                     weak_ptr_factory_.GetWeakPtr()));
  provider_->on_registry_ready().Post(FROM_HERE,
                                      initialized_components_barrier);
  key_distribution_info_provider.OnMaybeDownloadedComponentDataReady().Post(
      FROM_HERE, initialized_components_barrier);
}

MultiCaptureUsageIndicatorService::AllowListedAppNames
MultiCaptureUsageIndicatorService::GetInstalledAndAllowlistedAppNames() const {
  CHECK(provider_);

  const std::vector<std::string>
      skip_capture_notification_bundle_ids_allowlist =
          web_app::IwaKeyDistributionInfoProvider::GetInstance()
              .GetSkipMultiCaptureNotificationBundleIds();

  std::map<webapps::AppId, std::string> future_capture_notification_apps;
  std::map<webapps::AppId, std::string> future_capture_no_notification_apps;
  std::map<webapps::AppId, std::string> current_capture_notification_apps;
  for (const base::Value& allowlisted_app_value :
       multi_screen_capture_allow_list_on_login_) {
    if (!allowlisted_app_value.is_string()) {
      continue;
    }

    const GURL allowlisted_app_url(allowlisted_app_value.GetString());
    web_app::WebAppRegistrar& registrar = provider_->registrar_unsafe();
    const std::optional<webapps::AppId> app_id =
        registrar.FindBestAppWithUrlInScope(
            allowlisted_app_url, web_app::WebAppFilter::IsIsolatedApp());

    // App isn't installed yet.
    if (!app_id) {
      continue;
    }

    const bool can_skip_active_notification =
        base::Contains(skip_capture_notification_bundle_ids_allowlist,
                       allowlisted_app_url.host());
    const bool is_currently_capturing =
        base::Contains(started_captures_, *app_id);
    const std::string app_name = registrar.GetAppShortName(*app_id);
    if (can_skip_active_notification) {
      future_capture_no_notification_apps[*app_id] = app_name;
    } else if (is_currently_capturing) {
      current_capture_notification_apps[*app_id] = app_name;
    } else {
      future_capture_notification_apps[*app_id] = app_name;
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

  // TODO(crbug.com/428931746): Check that we only close an actual capturing
  // notification.
  // TODO(crbug.com/428931746): Also close the future notification if it moves
  // over to active notification.
  notification_display_service_->Close(
      NotificationHandler::Type::ANNOUNCEMENT,
      kPrivacyIndicatorsMultiCaptureLoginNotificationId);

  const message_center::Notification notification =
      CreateFutureCaptureNotification(apps);
  notification_display_service_->Display(
      NotificationHandler::Type::ANNOUNCEMENT, notification,
      /*metadata=*/nullptr);
}

void MultiCaptureUsageIndicatorService::ShowActiveMultiCaptureNotifications(
    const AllowListedAppNames& apps) {
  web_app::WebAppRegistrar& registrar = provider_->registrar_unsafe();
  std::vector<std::pair<webapps::AppId, std::string>> apps_to_notify;
  for (const auto& [app_id, _] : started_captures_) {
    if (!registrar.GetAppById(app_id)) {
      continue;
    }

    if (!apps.current_capture_notification_apps.contains(app_id)) {
      continue;
    }

    // TODO(crbug.com/428901090): Don't add notifications here that were already
    // shown once.
    apps_to_notify.emplace_back(app_id, registrar.GetAppShortName(app_id));
  }

  std::sort(apps_to_notify.begin(), apps_to_notify.end(),
            [](const auto& app_1, const auto& app_2) {
              return app_1.second < app_2.second;
            });

  bool reuse_future_notification_id = ShouldReuseFutureNotificationId(apps);
  for (const auto& [app_id, app_name] : apps_to_notify) {
    notification_display_service_->Display(
        NotificationHandler::Type::ANNOUNCEMENT,
        CreateActiveCaptureNotification(app_id, app_name,
                                        reuse_future_notification_id),
        /*metadata=*/nullptr);
    reuse_future_notification_id = false;
  }
}

void MultiCaptureUsageIndicatorService::RefreshNotifications() {
  const AllowListedAppNames apps = GetInstalledAndAllowlistedAppNames();
  if (apps.future_capture_notification_apps.empty() &&
      apps.future_capture_no_notification_apps.empty() &&
      apps.current_capture_notification_apps.empty()) {
    return;
  }

  ShowFutureMultiCaptureNotification(apps);
  ShowActiveMultiCaptureNotifications(apps);
}

}  // namespace multi_capture
