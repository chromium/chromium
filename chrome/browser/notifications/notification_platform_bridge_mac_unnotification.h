// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UNNOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UNNOTIFICATION_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/notifications/alert_dispatcher_mac.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/common/notifications/notification_image_retainer.h"
#include "chrome/services/mac_notifications/public/cpp/notification_category_manager.h"

@class UNNotificationBuilder;
@class UNNotificationCenterDelegate;
@class UNUserNotificationCenter;

namespace message_center {
class Notification;
}  // namespace message_center

// This class is an implementation of NotificationPlatformBridge that will
// send platform notifications to the MacOS notification center for devices
// running on macOS 10.14+.
class API_AVAILABLE(macosx(10.14)) NotificationPlatformBridgeMacUNNotification
    : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeMacUNNotification(
      UNUserNotificationCenter* notification_center,
      id<AlertDispatcher> alert_dispatcher);
  NotificationPlatformBridgeMacUNNotification(
      const NotificationPlatformBridgeMacUNNotification&) = delete;
  NotificationPlatformBridgeMacUNNotification& operator=(
      const NotificationPlatformBridgeMacUNNotification&) = delete;
  ~NotificationPlatformBridgeMacUNNotification() override;

  // NotificationPlatformBridge implementation.
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;

  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;

  // Request permission to send notifications.
  void RequestPermission();

 private:
  // Remove the closed alert notification.
  void DoCloseAlert(Profile* profile, const std::string& notification_id);

  // Called when a notification has been closed from the notification center.
  void OnNotificationClosed(std::string notification_id);

  // Process notification request that got delivered successfully.
  void DeliveredSuccessfully(
      const std::string& notification_id,
      base::scoped_nsobject<UNNotificationBuilder> builder);

  // Determine whether to start synchronization process of notifications or not.
  void MaybeStartSynchronization();

  // Perform the synchronization process of notifications.
  void SynchronizeNotifications();

  // Performs the sync part from |SynchronizeNotifications| on a UI thread task
  // runner.
  void DoSynchronizeNotifications(base::flat_set<std::string> notification_ids);

  // Called with |alert_ids| for |profile| from the |alert_dispatcher_|.
  void DidGetDisplayedAlerts(Profile* profile,
                             GetDisplayedNotificationsCallback callback,
                             std::set<std::string> alert_ids,
                             bool supports_synchronization);

  // Called with all |alert_ids| for all profiles from the |alert_dispatcher_|.
  void DidGetAllDisplayedAlerts(
      base::flat_set<MacNotificationIdentifier> alert_ids);

  // Closes all notifications for the given |profile|.
  void CloseAllNotificationsForProfile(Profile* profile);

  // Cocoa class that receives callbacks from the UNUserNotificationCenter.
  base::scoped_nsobject<UNNotificationCenterDelegate> delegate_;

  // The notification center to use for local banner notifications,
  // this can be overridden in tests.
  base::scoped_nsobject<UNUserNotificationCenter> notification_center_;

  // The object in charge of dispatching alert notifications.
  base::scoped_nsprotocol<id<AlertDispatcher>> alert_dispatcher_;

  // An object that keeps temp files alive long enough for macOS to pick up.
  NotificationImageRetainer image_retainer_;

  // An object that carries the delivered notifications to compare with the
  // actual delivered notifications in the notification center.
  base::scoped_nsobject<NSMutableDictionary> delivered_notifications_;

  // An object used to synchronize the notifications by polling over them every
  // |kSynchronizationInterval| minutes.
  base::RepeatingTimer synchronize_displayed_notifications_timer_;

  // Category manager for action buttons.
  NotificationCategoryManager category_manager_;

  base::WeakPtrFactory<NotificationPlatformBridgeMacUNNotification>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UNNOTIFICATION_H_
