// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATE_H_

#include <optional>
#include <string>

// The interface that a NotificationPlatformBridge uses to pass back information
// and interactions from the native notification system.
// TODO(estade): This should be implemented by NativeNotificationDisplayService,
// and used by other platforms' NotificationPlatformBridge implementations. See
// http://crbug.com/776443
class NotificationPlatformBridgeDelegate {
 public:
  // To be called when a notification is closed. Each notification can be closed
  // at most once.
  virtual void HandleNotificationClosed(const std::string& id,
                                        bool by_user) = 0;

  // To be called when the body of a notification is clicked.
  virtual void HandleNotificationClicked(const std::string& id) = 0;

  // To be called when a button in a notification is clicked.
  virtual void HandleNotificationButtonClicked(
      const std::string& id,
      int button_index,
      const std::optional<std::u16string>& reply) = 0;

  // To be called when the settings button in a notification is clicked.
  virtual void HandleNotificationSettingsButtonClicked(
      const std::string& id) = 0;

  // To be called when a notification (source) should be disabled.
  virtual void DisableNotification(const std::string& id) = 0;

 protected:
  virtual ~NotificationPlatformBridgeDelegate() = default;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_DELEGATE_H_
