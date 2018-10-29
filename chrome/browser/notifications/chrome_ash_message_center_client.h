// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "base/unguessable_token.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/message_center/public/cpp/notifier_id.h"

// This class serves as Chrome's AshMessageCenterClient, as well as the
// NotificationPlatformBridge for ChromeOS. It dispatches notifications to Ash
// and handles interactions with those notifications, plus it keeps track of
// NotifierControllers to provide notifier settings information to Ash (visible
// in NotifierSettingsView).
class ChromeAshMessageCenterClient : public ash::mojom::AshMessageCenterClient,
                                     public NotifierController::Observer {
 public:
  explicit ChromeAshMessageCenterClient(
      NotificationPlatformBridgeDelegate* delegate);

  ~ChromeAshMessageCenterClient() override;

  void Display(const message_center::Notification& notification);
  void Close(const std::string& notification_id);

  // ash::mojom::AshMessageCenterClient:
  void HandleNotificationClosed(const base::UnguessableToken& display_token,
                                bool by_user) override;
  void HandleNotificationClicked(const std::string& id) override;
  void HandleNotificationButtonClicked(
      const std::string& id,
      int button_index,
      const base::Optional<base::string16>& reply) override;
  void HandleNotificationSettingsButtonClicked(const std::string& id) override;
  void DisableNotification(const std::string& id) override;
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled) override;
  void GetNotifierList(GetNotifierListCallback callback) override;
  void GetArcAppIdByPackageName(
      const std::string& package_name,
      GetArcAppIdByPackageNameCallback callback) override;
  void ShowLockScreenNotificationSettings() override;

  // NotifierController::Observer:
  void OnIconImageUpdated(const message_center::NotifierId& notifier_id,
                          const gfx::ImageSkia& icon) override;
  void OnNotifierEnabledChanged(const message_center::NotifierId& notifier_id,
                                bool enabled) override;

  // Flushs |binding_|.
  static void FlushForTesting();

 private:
  NotificationPlatformBridgeDelegate* delegate_;

  // A mapping from display token to notification ID. The display token is
  // generated each time a notification is shown (even if a notification is
  // displayed more than once). This allows |this| to drop out-of-order
  // HandleNotificationClosed() calls (i.e. those that arrive after the
  // notification has already been re-displayed/updated and refer to an earlier
  // notification).
  std::map<base::UnguessableToken, std::string> displayed_notifications_;

  // Notifier source for each notifier type.
  std::map<message_center::NotifierId::NotifierType,
           std::unique_ptr<NotifierController>>
      sources_;

  ash::mojom::AshMessageCenterControllerPtr controller_;
  mojo::AssociatedBinding<ash::mojom::AshMessageCenterClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAshMessageCenterClient);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_CHROME_ASH_MESSAGE_CENTER_CLIENT_H_
