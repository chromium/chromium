// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_

#include "ash/public/cpp/notifier_settings_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegate.h"
#include "chrome/browser/notifications/notifier_controller.h"

// Helper for NotificationPlatformBridgeChromeOs. Sends notifications to Ash
// and handles interactions with those notifications, plus it keeps track of
// NotifierControllers to provide notifier settings information to Ash (visible
// in NotifierSettingsView). With Lacros, runs in the ash-chrome process.
class ChromeAshMessageCenterClient : public NotificationPlatformBridge,
                                     public ash::NotifierSettingsController,
                                     public NotifierController::Observer {
 public:
  explicit ChromeAshMessageCenterClient(
      NotificationPlatformBridgeDelegate* delegate);
  ChromeAshMessageCenterClient(const ChromeAshMessageCenterClient&) = delete;
  ChromeAshMessageCenterClient& operator=(const ChromeAshMessageCenterClient&) =
      delete;
  ~ChromeAshMessageCenterClient() override;

  // NotificationPlatformBridge:
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
  void DisplayServiceShutDown(Profile* profile) override {}

  // ash::NotifierSettingsController:
  void GetNotifiers() override;
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled) override;
  void AddNotifierSettingsObserver(
      ash::NotifierSettingsObserver* observer) override;
  void RemoveNotifierSettingsObserver(
      ash::NotifierSettingsObserver* observer) override;

  // NotifierController::Observer:
  void OnIconImageUpdated(const message_center::NotifierId& notifier_id,
                          const gfx::ImageSkia& icon) override;
  void OnNotifierEnabledChanged(const message_center::NotifierId& notifier_id,
                                bool enabled) override;

 private:
  raw_ptr<NotificationPlatformBridgeDelegate> delegate_;

  // Notifier source for each notifier type.
  std::map<message_center::NotifierType, std::unique_ptr<NotifierController>>
      sources_;

  base::ObserverList<ash::NotifierSettingsObserver> notifier_observers_;

  base::WeakPtrFactory<ChromeAshMessageCenterClient> weak_ptr_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
