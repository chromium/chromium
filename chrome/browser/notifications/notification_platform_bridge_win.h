// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_

#include <windows.ui.notifications.h>
#include <wrl/client.h>

#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/common/notifications/notification_operation.h"

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
  NotificationPlatformBridgeWin(const NotificationPlatformBridgeWin&) = delete;
  NotificationPlatformBridgeWin& operator=(
      const NotificationPlatformBridgeWin&) = delete;
  ~NotificationPlatformBridgeWin() override;

  // NotificationPlatformBridge implementation.
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void GetDisplayedForOrigin(
      Profile* profile,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;

  // Handles notification activation encoded in |command_line| from the
  // notification_helper process.
  // Returns false if |command_line| does not contain a valid
  // notification-launch-id switch.
  static bool HandleActivation(const base::CommandLine& command_line);

  // Checks if system notifications are enabled.
  static bool SystemNotificationEnabled();

  // Struct used to build the key to identify the notifications.
  struct NotificationKeyType {
    std::string profile_id;
    std::string notification_id;
    std::wstring app_user_model_id;  // Either browser aumi or web app aumi.

    bool operator<(const NotificationKeyType& key) const {
      return profile_id < key.profile_id ||
             (profile_id == key.profile_id &&
              notification_id < key.notification_id);
    }
  };

 private:
  friend class NotificationPlatformBridgeWinImpl;
  friend class NotificationPlatformBridgeWinTest;
  friend class NotificationPlatformBridgeWinAppInstalledTest;
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinTest, Suppress);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, GetDisplayed);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, HandleEvent);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, HandleSettings);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest,
                           DisplayWithFakeAC);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest,
                           DisplayWebAppNotificationWithFakeAC);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest,
                           SynchronizeNotifications);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest,
                           SynchronizeNotificationsAfterClose);

  void SynchronizeNotificationsForTesting();

  // Simulates a click/dismiss event. Only for use in testing.
  // Note: Ownership of |notification| and |args| is retained by the caller.
  void ForwardHandleEventForTesting(
      NotificationOperation operation,
      ABI::Windows::UI::Notifications::IToastNotification* notification,
      ABI::Windows::UI::Notifications::IToastActivatedEventArgs* args,
      const std::optional<bool>& by_user);

  // Initializes the expected displayed notification map. For testing use only.
  void SetExpectedDisplayedNotificationsForTesting(
      std::map<NotificationKeyType, NotificationLaunchId>*
          expected_displayed_notification);

  // Initializes the displayed notification vector. Only for use in testing.
  void SetDisplayedNotificationsForTesting(
      std::vector<Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Notifications::IToastNotification>>* notifications);

  // Sets a Toast Notifier to use to display notifications, when run in a test.
  void SetNotifierForTesting(
      ABI::Windows::UI::Notifications::IToastNotifier* notifier);

  // Returns the map of notifications which should be currently displayed. For
  // testing use only.
  std::map<NotificationKeyType, NotificationLaunchId>
  GetExpectedDisplayedNotificationForTesting() const;

  // Obtain an IToastNotification interface from a given XML (provided by the
  // NotificationTemplateBuilder). For testing use only.
  Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotification>
  GetToastNotificationForTesting(
      const message_center::Notification& notification,
      const std::wstring& xml_template,
      const std::string& profile_id,
      const std::wstring& app_user_model_id,
      bool incognito);

  scoped_refptr<NotificationPlatformBridgeWinImpl> impl_;

  scoped_refptr<base::SequencedTaskRunner> notification_task_runner_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
