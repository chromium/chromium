// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"

#include "base/i18n/number_formatting.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Loads the profile and process the Notification response
void DoProcessMacNotificationResponse(
    NotificationCommon::Operation operation,
    NotificationHandler::Type type,
    const std::string& profileId,
    bool incognito,
    const GURL& origin,
    const std::string& notificationId,
    const base::Optional<int>& actionIndex,
    const base::Optional<base::string16>& reply,
    const base::Optional<bool>& byUser) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Profile ID can be empty for system notifications, which are not bound to a
  // profile, but system notifications are transient and thus not handled by
  // this NotificationPlatformBridge.
  // When transient notifications are supported, this should route the
  // notification response to the system NotificationDisplayService.
  DCHECK(!profileId.empty());

  ProfileManager* profileManager = g_browser_process->profile_manager();
  DCHECK(profileManager);

  profileManager->LoadProfile(
      profileId, incognito,
      base::Bind(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                 operation, type, origin, notificationId, actionIndex, reply,
                 byUser));
}

}  // namespace

base::string16 CreateMacNotificationTitle(
    const message_center::Notification& notification) {
  base::string16 title;
  // Show progress percentage if available. We don't support indeterminate
  // states on macOS native notifications.
  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS &&
      notification.progress() >= 0 && notification.progress() <= 100) {
    title += base::FormatPercent(notification.progress());
    title += base::UTF8ToUTF16(" - ");
  }
  title += notification.title();
  return title;
}

base::string16 CreateMacNotificationContext(
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

  base::string16 origin = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(notification.origin_url()),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  if (origin.size() <= maxCharacters)
    return origin;

  // Too long, use etld+1
  base::string16 etldplusone =
      base::UTF8ToUTF16(net::registry_controlled_domains::GetDomainAndRegistry(
          notification.origin_url(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  // localhost, raw IPs etc. are not handled by GetDomainAndRegistry.
  if (etldplusone.empty())
    return origin;

  return etldplusone;
}

bool VerifyMacNotificationData(NSDictionary* response) {
  if (![response
          objectForKey:notification_constants::kNotificationButtonIndex] ||
      ![response objectForKey:notification_constants::kNotificationOperation] ||
      ![response objectForKey:notification_constants::kNotificationId] ||
      ![response objectForKey:notification_constants::kNotificationProfileId] ||
      ![response objectForKey:notification_constants::kNotificationIncognito] ||
      ![response
          objectForKey:notification_constants::kNotificationCreatorPid] ||
      ![response objectForKey:notification_constants::kNotificationType]) {
    LOG(ERROR) << "Missing required key";
    return false;
  }

  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSString* notificationId =
      [response objectForKey:notification_constants::kNotificationId];
  NSString* profileId =
      [response objectForKey:notification_constants::kNotificationProfileId];
  NSNumber* notificationType =
      [response objectForKey:notification_constants::kNotificationType];
  NSNumber* creatorPid =
      [response objectForKey:notification_constants::kNotificationCreatorPid];

  if (creatorPid.unsignedIntValue != static_cast<NSInteger>(getpid())) {
    return false;
  }

  if (buttonIndex.intValue <
          notification_constants::kNotificationInvalidButtonIndex ||
      buttonIndex.intValue >=
          static_cast<int>(blink::kNotificationMaxActions)) {
    LOG(ERROR) << "Invalid number of buttons supplied " << buttonIndex.intValue;
    return false;
  }

  if (operation.unsignedIntValue > NotificationCommon::OPERATION_MAX) {
    LOG(ERROR) << operation.unsignedIntValue
               << " does not correspond to a valid operation.";
    return false;
  }

  if (notificationId.length <= 0) {
    LOG(ERROR) << "Notification Id is empty";
    return false;
  }

  if (profileId.length <= 0) {
    LOG(ERROR) << "ProfileId not provided";
    return false;
  }

  if (notificationType.unsignedIntValue >
      static_cast<unsigned int>(NotificationHandler::Type::MAX)) {
    LOG(ERROR) << notificationType.unsignedIntValue
               << " Does not correspond to a valid operation.";
    return false;
  }

  // Origin is not actually required but if it's there it should be a valid one.
  NSString* origin =
      [response objectForKey:notification_constants::kNotificationOrigin];
  if (origin && origin.length) {
    std::string notificationOrigin = base::SysNSStringToUTF8(origin);
    GURL url(notificationOrigin);
    if (!url.is_valid())
      return false;
  }

  return true;
}

void ProcessMacNotificationResponse(NSDictionary* response) {
  if (!VerifyMacNotificationData(response))
    return;

  NSNumber* buttonIndex =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];

  std::string notificationOrigin = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationOrigin]);
  std::string notificationId = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationId]);
  std::string profileId = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationProfileId]);
  NSNumber* isIncognito =
      [response objectForKey:notification_constants::kNotificationIncognito];
  NSNumber* notificationType =
      [response objectForKey:notification_constants::kNotificationType];

  base::Optional<int> actionIndex;
  if (buttonIndex.intValue !=
      notification_constants::kNotificationInvalidButtonIndex) {
    actionIndex = buttonIndex.intValue;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(DoProcessMacNotificationResponse,
                     static_cast<NotificationCommon::Operation>(
                         operation.unsignedIntValue),
                     static_cast<NotificationHandler::Type>(
                         notificationType.unsignedIntValue),
                     profileId, [isIncognito boolValue],
                     GURL(notificationOrigin), notificationId, actionIndex,
                     base::nullopt /* reply */, true /* byUser */));
}
