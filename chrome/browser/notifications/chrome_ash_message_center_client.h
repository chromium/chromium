// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_

#include "ash/public/cpp/notifier_settings_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"
#include "chrome/browser/notifications/notifier_controller.h"

// This class serves as Chrome's AshMessageCenterClient, as well as the
// NotificationPlatformBridge for ChromeOS. It dispatches notifications to Ash
// and handles interactions with those notifications, plus it keeps track of
// NotifierControllers to provide notifier settings information to Ash (visible
// in NotifierSettingsView).
class ChromeAshMessageCenterClient : public ash::NotifierSettingsController,
                                     public NotifierController::Observer {
 public:
  explicit ChromeAshMessageCenterClient(
      NotificationPlatformBridgeDelegate* delegate);

  ~ChromeAshMessageCenterClient() override;

  void Display(const message_center::Notification& notification);
  void Close(const std::string& notification_id);

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
  NotificationPlatformBridgeDelegate* delegate_;

  // Notifier source for each notifier type.
  std::map<message_center::NotifierType, std::unique_ptr<NotifierController>>
      sources_;

  base::ObserverList<ash::NotifierSettingsObserver> notifier_observers_;

  base::WeakPtrFactory<ChromeAshMessageCenterClient> weak_ptr_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeAshMessageCenterClient);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
