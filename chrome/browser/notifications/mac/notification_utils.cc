// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_utils.h"

#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/number_formatting.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notifications/notification_constants.h"
#include "chrome/common/notifications/notification_operation.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

void DisplayWebAppSettings(const webapps::AppId& web_app_id, Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "Profile not loaded correctly";
    return;
  }
  chrome::ShowWebAppSettings(
      profile, web_app_id,
      web_app::AppSettingsPageEntryPoint::kNotificationSettingsButton);
}

// Loads the profile and process the Notification response
void DoProcessMacNotificationResponse(
    mac_notifications::mojom::NotificationActionInfoPtr info,
    std::optional<webapps::AppId> web_app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  std::optional<int> action_index;
  if (info->button_index != kNotificationInvalidButtonIndex)
    action_index = info->button_index;

  auto operation = static_cast<NotificationOperation>(info->operation);
  ProfileManager::ProfileLoadedCallback callback =
      (operation == NotificationOperation::kSettings && web_app_id.has_value())
          ? base::BindOnce(&DisplayWebAppSettings, *web_app_id)
          : base::BindOnce(
                &NotificationDisplayServiceImpl::ProfileLoadedCallback,
                operation,
                static_cast<NotificationHandler::Type>(info->meta->type),
                std::move(info->meta->origin_url),
                std::move(info->meta->id->id), std::move(action_index),
                std::move(info->reply), /*by_user=*/true);
  profile_manager->LoadProfile(
      NotificationPlatformBridge::GetProfileBaseNameFromProfileId(
          info->meta->id->profile->id),
      info->meta->id->profile->incognito, std::move(callback));
}

// Get the user data directory.
std::string GetUserDataDir() {
  return base::PathService::CheckedGet(chrome::DIR_USER_DATA).value();
}

}  // namespace

std::u16string CreateMacNotificationTitle(
    const message_center::Notification& notification) {
  std::u16string title;
  // Show progress percentage if available. We don't support indeterminate
  // states on macOS native notifications.
  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS &&
      notification.progress() >= 0 && notification.progress() <= 100) {
    title += base::FormatPercent(notification.progress());
    title += u" - ";
  }
  title += notification.title();
  return title;
}

std::u16string CreateMacNotificationContext(
    bool isPersistent,
    const message_center::Notification& notification,
    bool requiresAttribution) {
  if (!requiresAttribution)
    return notification.context_message();

  // Mac OS notifications don't provide a good way to elide the domain (or tell
  // you the maximum width of the subtitle field). We have experimentally
  // determined the maximum number of characters that fit using the widest
  // possible character (m). If the domain fits in those character we show it
  // completely. Otherwise we use eTLD + 1.

  // These numbers have been obtained through experimentation on various
  // Mac OS platforms.

  constexpr size_t kMaxDomainLengthAlert = 19;
  constexpr size_t kMaxDomainLengthBanner = 28;

  size_t maxCharacters =
      isPersistent ? kMaxDomainLengthAlert : kMaxDomainLengthBanner;

  std::u16string origin = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(notification.origin_url()),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  if (origin.size() <= maxCharacters)
    return origin;

  // Too long, use etld+1
  std::u16string etldplusone =
      base::UTF8ToUTF16(net::registry_controlled_domains::GetDomainAndRegistry(
          notification.origin_url(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  // localhost, raw IPs etc. are not handled by GetDomainAndRegistry.
  if (etldplusone.empty())
    return origin;

  return etldplusone;
}

bool VerifyMacNotificationData(
    const mac_notifications::mojom::NotificationActionInfoPtr& info) {
  if (!info || !info->meta || !info->meta->id || !info->meta->id->profile) {
    LOG(ERROR) << "Missing required data";
    return false;
  }

  if (info->meta->user_data_dir != GetUserDataDir()) {
    return false;
  }

  if (info->button_index < kNotificationInvalidButtonIndex ||
      info->button_index >= static_cast<int>(blink::kNotificationMaxActions)) {
    LOG(ERROR) << "Invalid number of buttons supplied " << info->button_index;
    return false;
  }

  if (info->meta->id->id.empty()) {
    LOG(ERROR) << "Notification Id is empty";
    return false;
  }

  if (info->meta->id->profile->id.empty()) {
    LOG(ERROR) << "ProfileId not provided";
    return false;
  }

  if (info->meta->type > static_cast<int>(NotificationHandler::Type::MAX)) {
    LOG(ERROR) << info->meta->type
               << " Does not correspond to a valid operation.";
    return false;
  }

  // Origin is not actually required but if it's there it should be a valid one.
  if (!info->meta->origin_url.is_empty() && !info->meta->origin_url.is_valid())
    return false;

  return true;
}

void ProcessMacNotificationResponse(
    mac_notifications::NotificationStyle notification_style,
    mac_notifications::mojom::NotificationActionInfoPtr info,
    std::optional<webapps::AppId> web_app_id) {
  bool is_valid = VerifyMacNotificationData(info);
  if (!is_valid)
    return;

  std::optional<int> actionIndex;
  if (info->button_index != kNotificationInvalidButtonIndex)
    actionIndex = info->button_index;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(DoProcessMacNotificationResponse,
                                std::move(info), web_app_id));
}

bool IsAlertNotificationMac(const message_center::Notification& notification) {
  // Check if the |notification| should be shown as alert.
  return notification.never_timeout() ||
         notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS;
}

mac_notifications::mojom::NotificationPtr CreateMacNotification(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification) {
  auto profile_identifier = mac_notifications::mojom::ProfileIdentifier::New(
      NotificationPlatformBridge::GetProfileId(profile),
      profile->IsOffTheRecord());
  auto notification_identifier =
      mac_notifications::mojom::NotificationIdentifier::New(
          notification.id(), std::move(profile_identifier));

  auto meta = mac_notifications::mojom::NotificationMetadata::New(
      std::move(notification_identifier), static_cast<int>(notification_type),
      notification.origin_url(), GetUserDataDir());

  std::vector<mac_notifications::mojom::NotificationActionButtonPtr> buttons;
  for (const message_center::ButtonInfo& button : notification.buttons()) {
    buttons.push_back(mac_notifications::mojom::NotificationActionButton::New(
        button.title, button.placeholder));
  }

  bool is_alert = IsAlertNotificationMac(notification);
  bool requires_attribution =
      notification.context_message().empty() &&
      notification_type != NotificationHandler::Type::EXTENSION;

  std::u16string body = notification.items().empty()
                            ? notification.message()
                            : (notification.items().at(0).title() + u" - " +
                               notification.items().at(0).message());

  return mac_notifications::mojom::Notification::New(
      std::move(meta), CreateMacNotificationTitle(notification),
      CreateMacNotificationContext(is_alert, notification,
                                   requires_attribution),
      std::move(body), notification.renotify(),
      notification.should_show_settings_button(), std::move(buttons),
      notification.icon().Rasterize(
          ThemeServiceFactory::GetForProfile(profile)->GetColorProvider()));
}
