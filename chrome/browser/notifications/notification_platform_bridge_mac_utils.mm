// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"

#include "base/feature_list.h"
#include "base/i18n/number_formatting.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_metrics.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
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
    const base::Optional<std::u16string>& reply,
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
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     operation, type, origin, notificationId, actionIndex,
                     reply, byUser));
}

// Implements the version check to determine if alerts are supported. Do not
// call this method directly as SysInfo::OperatingSystemVersionNumbers might be
// an expensive call. Instead use SupportsAlerts which caches this value.
bool MacOSSupportsXPCAlertsImpl() {
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  // Allow alerts on all versions except 10.15.0, 10.15.1 & 10.15.2.
  // See crbug.com/1007418 for details.
  return major != 10 || minor != 15 || bugfix > 2;
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

bool VerifyMacNotificationData(NSDictionary* response) {
  if (![response
          objectForKey:notification_constants::kNotificationButtonIndex] ||
      ![response objectForKey:notification_constants::kNotificationOperation] ||
      ![response objectForKey:notification_constants::kNotificationId] ||
      ![response objectForKey:notification_constants::kNotificationProfileId] ||
      ![response objectForKey:notification_constants::kNotificationIncognito] ||
      ![response
          objectForKey:notification_constants::kNotificationCreatorPid] ||
      ![response objectForKey:notification_constants::kNotificationType] ||
      ![response objectForKey:notification_constants::kNotificationIsAlert]) {
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
  bool isAlert = [[response
      objectForKey:notification_constants::kNotificationIsAlert] boolValue];
  bool isValid = VerifyMacNotificationData(response);
  LogMacNotificationActionReceived(isAlert, isValid);

  if (!isValid)
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

bool MacOSSupportsXPCAlerts() {
  // Cache result as SysInfo::OperatingSystemVersionNumbers might be expensive.
  static bool supportsAlerts = MacOSSupportsXPCAlertsImpl();
  return supportsAlerts;
}

bool IsAlertNotificationMac(const message_center::Notification& notification) {
  // If we show alerts via an XPC service, check if that's possible.
  bool should_use_xpc =
      !base::FeatureList::IsEnabled(features::kNotificationsViaHelperApp);
  if (should_use_xpc && !MacOSSupportsXPCAlerts())
    return false;

  // Check if the |notification| should be shown as alert.
  return notification.never_timeout() ||
         notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS;
}
