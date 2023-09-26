// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATOR_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

// This class is responsible for delegating notification events to either the
// system NotificationPlatformBridge or fall back to Chrome's own message
// center implementation. This can happen if there is no system support for
// notifications on this platform (or it has been disabled via flags). We also
// delegate to the message center if the given notification type is not
// supported on the system bridge.
class NotificationPlatformBridgeDelegator {
 public:
  NotificationPlatformBridgeDelegator(Profile* profile,
                                      base::OnceClosure ready_callback);
  NotificationPlatformBridgeDelegator(
      const NotificationPlatformBridgeDelegator&) = delete;
  NotificationPlatformBridgeDelegator& operator=(
      const NotificationPlatformBridgeDelegator&) = delete;
  virtual ~NotificationPlatformBridgeDelegator();

  virtual void Display(NotificationHandler::Type notification_type,
                       const message_center::Notification& notification,
                       std::unique_ptr<NotificationCommon::Metadata> metadata);

  virtual void Close(NotificationHandler::Type notification_type,
                     const std::string& notification_id);

  virtual void GetDisplayed(GetDisplayedNotificationsCallback callback) const;
  virtual void GetDisplayedForOrigin(
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const;

  virtual void DisplayServiceShutDown();

 private:
  // Returns the NotificationPlatformBridge to use for |type|. This method is
  // expected to return a valid bridge, either the system or message center one.
  NotificationPlatformBridge* GetBridgeForType(NotificationHandler::Type type);

  // Called when the |system_bridge_| may have been initialized.
  void OnSystemNotificationPlatformBridgeReady(bool success);

  raw_ptr<Profile> profile_;

  // Bridge responsible for displaying notifications on the platform. The
  // message center's bridge is maintained for platforms where it is available.
  raw_ptr<NotificationPlatformBridge> message_center_bridge_;
  raw_ptr<NotificationPlatformBridge> system_bridge_;
  base::OnceClosure ready_callback_;

  base::WeakPtrFactory<NotificationPlatformBridgeDelegator> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATOR_H_
