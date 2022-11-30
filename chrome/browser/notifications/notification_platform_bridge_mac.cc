// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/notifications/mac_notification_provider_factory.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_dispatcher_mojo.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

NotificationPlatformBridgeMac::NotificationPlatformBridgeMac(
    std::unique_ptr<NotificationDispatcherMac> banner_dispatcher,
    std::unique_ptr<NotificationDispatcherMac> alert_dispatcher)
    : banner_dispatcher_(std::move(banner_dispatcher)),
      alert_dispatcher_(std::move(alert_dispatcher)) {}

NotificationPlatformBridgeMac::~NotificationPlatformBridgeMac() {
  // TODO(miguelg) do not remove banners if possible.
  banner_dispatcher_->CloseAllNotifications();
  alert_dispatcher_->CloseAllNotifications();
}

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  auto banner_dispatcher = std::make_unique<NotificationDispatcherMojo>(
      std::make_unique<MacNotificationProviderFactory>(/*in_process=*/true));
  auto alert_dispatcher = std::make_unique<NotificationDispatcherMojo>(
      std::make_unique<MacNotificationProviderFactory>(/*in_process=*/false));

  return std::make_unique<NotificationPlatformBridgeMac>(
      std::move(banner_dispatcher), std::move(alert_dispatcher));
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

void NotificationPlatformBridgeMac::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  bool is_alert = IsAlertNotificationMac(notification);
  NotificationDispatcherMac* dispatcher =
      is_alert ? alert_dispatcher_.get() : banner_dispatcher_.get();
  dispatcher->DisplayNotification(notification_type, profile, notification);
}

void NotificationPlatformBridgeMac::Close(Profile* profile,
                                          const std::string& notification_id) {
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();

  banner_dispatcher_->CloseNotificationWithId(
      {notification_id, profile_id, incognito});
  alert_dispatcher_->CloseNotificationWithId(
      {notification_id, profile_id, incognito});
}

void NotificationPlatformBridgeMac::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();

  auto notifications = std::make_unique<std::set<std::string>>();
  std::set<std::string>* notifications_ptr = notifications.get();
  auto barrier_closure = base::BarrierClosure(
      2, base::BindOnce(
             [](std::unique_ptr<std::set<std::string>> notifications,
                GetDisplayedNotificationsCallback callback) {
               std::move(callback).Run(std::move(*notifications),
                                       /*supports_synchronization=*/true);
             },
             std::move(notifications), std::move(callback)));

  auto get_notifications_callback = base::BindRepeating(
      [](std::set<std::string>* notifications_ptr, base::OnceClosure callback,
         std::set<std::string> notifications, bool supports_synchronization) {
        notifications_ptr->insert(notifications.begin(), notifications.end());
        std::move(callback).Run();
      },
      notifications_ptr, barrier_closure);

  banner_dispatcher_->GetDisplayedNotificationsForProfileId(
      profile_id, incognito, get_notifications_callback);
  alert_dispatcher_->GetDisplayedNotificationsForProfileId(
      profile_id, incognito, get_notifications_callback);
}

void NotificationPlatformBridgeMac::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true);
}

void NotificationPlatformBridgeMac::DisplayServiceShutDown(Profile* profile) {
  // Close all alerts and banners for |profile| on shutdown. We have to clean up
  // here instead of the destructor as mojo messages won't be delivered from
  // there as it's too late in the shutdown process. If the profile is null it
  // was the SystemNotificationHelper instance but we never show notifications
  // without a profile (Type::TRANSIENT) on macOS, so nothing to do here.
  if (profile)
    CloseAllNotificationsForProfile(profile);
}

void NotificationPlatformBridgeMac::CloseAllNotificationsForProfile(
    Profile* profile) {
  DCHECK(profile);
  std::string profile_id = GetProfileId(profile);
  bool incognito = profile->IsOffTheRecord();

  banner_dispatcher_->CloseNotificationsWithProfileId(profile_id, incognito);
  alert_dispatcher_->CloseNotificationsWithProfileId(profile_id, incognito);
}
