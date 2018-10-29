// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_

#include <string>

#include <windows.ui.notifications.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

namespace base {
class CommandLine;
class SequencedTaskRunner;
}

class NotificationPlatformBridgeWinImpl;
class NotificationTemplateBuilder;

// Implementation of the NotificationPlatformBridge for Windows 10 Anniversary
// Edition and beyond, delegating display of notifications to the Action Center.
class NotificationPlatformBridgeWin : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeWin();
  ~NotificationPlatformBridgeWin() override;

  // NotificationPlatformBridge implementation.
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;

  // Handles notification activation encoded in |command_line| from the
  // notification_helper process.
  // Returns false if |command_line| does not contain a valid
  // notification-launch-id switch.
  static bool HandleActivation(const base::CommandLine& command_line);

  // Checks if native notification is enabled.
  static bool NativeNotificationEnabled();

 private:
  friend class NotificationPlatformBridgeWinImpl;
  friend class NotificationPlatformBridgeWinTest;
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinTest, Suppress);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, GetDisplayed);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, HandleEvent);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, HandleSettings);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest,
                           DisplayWithMockAC);

  // Simulates a click/dismiss event. Only for use in testing.
  // Note: Ownership of |notification| and |args| is retained by the caller.
  void ForwardHandleEventForTesting(
      NotificationCommon::Operation operation,
      ABI::Windows::UI::Notifications::IToastNotification* notification,
      ABI::Windows::UI::Notifications::IToastActivatedEventArgs* args,
      const base::Optional<bool>& by_user);

  // Initializes the displayed notification vector. Only for use in testing.
  void SetDisplayedNotificationsForTesting(
      std::vector<Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Notifications::IToastNotification>>* notifications);

  // Sets a Toast Notifier to use to display notifications, when run in a test.
  void SetNotifierForTesting(
      ABI::Windows::UI::Notifications::IToastNotifier* notifier);

  // Obtain an IToastNotification interface from a given XML (provided by the
  // NotificationTemplateBuilder). For testing use only.
  Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotification>
  GetToastNotificationForTesting(
      const message_center::Notification& notification,
      const NotificationTemplateBuilder& notification_template_builder,
      const std::string& profile_id,
      bool incognito);

  scoped_refptr<NotificationPlatformBridgeWinImpl> impl_;

  scoped_refptr<base::SequencedTaskRunner> notification_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWin);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
