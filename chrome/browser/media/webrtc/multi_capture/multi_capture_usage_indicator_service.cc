// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_usage_indicator_service.h"

#include <memory>
#include <string>

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/notification_utils.h"
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

// TODO(crbug.com/424104840): Change notification message in case that there are
// apps that are allowed the active screen capture notification.
std::u16string CreateFutureCaptureNotificationMessage(
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& app_names) {
  CHECK(!app_names.show_capture_notification_apps.empty() ||
        !app_names.skip_capture_notification_apps.empty());

  std::u16string message;
  if (!app_names.show_capture_notification_apps.empty() &&
      !app_names.skip_capture_notification_apps.empty()) {
    message = l10n_util::GetStringUTF16(
        IDS_MULTI_CAPTURE_MAY_CAPTURE_SOME_NOTIFY_NOTIFICATION_MESSAGE);
  } else if (!app_names.show_capture_notification_apps.empty()) {
    message = l10n_util::GetStringUTF16(
        IDS_MULTI_CAPTURE_MAY_CAPTURE_ALL_NOTIFY_NOTIFICATION_MESSAGE);
  } else {
    message = l10n_util::GetStringUTF16(
        IDS_MULTI_CAPTURE_MAY_CAPTURE_NONE_NOTIFY_NOTIFICATION_MESSAGE);
  }

  std::vector<std::string> merged_app_names =
      app_names.skip_capture_notification_apps;
  base::Extend(merged_app_names, app_names.show_capture_notification_apps);
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
    const MultiCaptureUsageIndicatorService::AllowListedAppNames& app_names) {
  CHECK(!app_names.show_capture_notification_apps.empty() ||
        !app_names.skip_capture_notification_apps.empty());

  message_center::RichNotificationData optional_fields;
  // Make the notification low priority so that it is silently added (no
  // popup).
  optional_fields.priority = message_center::LOW_PRIORITY;
  optional_fields.pinned = true;
  // TODO(crbug.com/424102053): Replace with finalized icon.
  optional_fields.vector_small_image = &vector_icons::kScreenShareIcon;

  message_center::Notification notification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kPrivacyIndicatorsMultiCaptureLoginNotificationId, /*title=*/u"",
      /*message=*/
      CreateFutureCaptureNotificationMessage(app_names),
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

}  // namespace

MultiCaptureUsageIndicatorService::AllowListedAppNames::AllowListedAppNames(
    std::vector<std::string> show_capture_notification_apps,
    std::vector<std::string> skip_capture_notification_apps)
    : show_capture_notification_apps(std::move(show_capture_notification_apps)),
      skip_capture_notification_apps(
          std::move(skip_capture_notification_apps)) {}
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

void MultiCaptureUsageIndicatorService::ShowUsageIndicatorsOnStart() {
  // Fetch the initial value of the multi screen capture allowlist for later
  // matching to prevent dynamic refresh. We intentionally break dynamic
  // refresh as it is not possible to add further screen capture apps after
  // session start due to privacy constraints.
  multi_screen_capture_allow_list_on_login_ =
      pref_service_
          ->GetList(capture_policy::kManagedMultiScreenCaptureAllowedForUrls)
          .Clone();

  if (provider_->on_registry_ready().is_signaled()) {
    ShowFutureMultiCaptureNotification();
    return;
  }

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&MultiCaptureUsageIndicatorService::
                                    ShowFutureMultiCaptureNotification,
                                weak_ptr_factory_.GetWeakPtr()));
}

MultiCaptureUsageIndicatorService::AllowListedAppNames
MultiCaptureUsageIndicatorService::GetInstalledAndAllowlistedAppNames() const {
  CHECK(provider_);

  const std::vector<std::string>
      skip_capture_notification_bundle_ids_allowlist =
          web_app::IwaKeyDistributionInfoProvider::GetInstance()
              .GetSkipMultiCaptureNotificationBundleIds();

  std::vector<std::string> show_capture_notification_apps;
  std::vector<std::string> skip_capture_notification_apps;
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

    if (base::Contains(skip_capture_notification_bundle_ids_allowlist,
                       allowlisted_app_url.host())) {
      skip_capture_notification_apps.push_back(
          registrar.GetAppShortName(*app_id));
    } else {
      show_capture_notification_apps.push_back(
          registrar.GetAppShortName(*app_id));
    }
  }

  return {show_capture_notification_apps, skip_capture_notification_apps};
}

// TODO(crbug.com/424103935): Call again when a new app is installed that is
// already on the screen capture allowlist on session start.
void MultiCaptureUsageIndicatorService::ShowFutureMultiCaptureNotification() {
  const AllowListedAppNames app_names = GetInstalledAndAllowlistedAppNames();
  if (app_names.show_capture_notification_apps.empty() &&
      app_names.skip_capture_notification_apps.empty()) {
    return;
  }

  const message_center::Notification notification =
      CreateFutureCaptureNotification(app_names);
  notification_display_service_->Display(
      NotificationHandler::Type::ANNOUNCEMENT, notification,
      /*metadata=*/nullptr);
}

}  // namespace multi_capture
