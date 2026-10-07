// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_APP_NOTIFICATION_CONTROLLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_APP_NOTIFICATION_CONTROLLER_H_

#include <optional>
#include <string>
#include <variant>

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {
namespace eche_app {

// Controller class to show notifications.
class EcheAppNotificationController {
 public:
  EcheAppNotificationController(
      const AccountId& account_id,
      const base::RepeatingClosure& relaunch_callback);
  virtual ~EcheAppNotificationController();

  EcheAppNotificationController(const EcheAppNotificationController&) = delete;
  EcheAppNotificationController& operator=(
      const EcheAppNotificationController&) = delete;

  // Shows the notification when screen lock is already enabled on the phone,
  // but the ChromeOS is not enabled.
  void ShowScreenLockNotification(const std::u16string& title);
  // Shows the notification which was generated from WebUI and carry title and
  // message.
  void ShowNotificationFromWebUI(
      const std::optional<std::u16string>& title,
      const std::optional<std::u16string>& message,
      std::variant<LaunchAppHelper::NotificationInfo::NotificationType,
                   mojom::WebNotificationType> type);

  // Close the notifiication according to id
  void CloseNotification(const std::string& notification_id);

  // Close the notifiications about coonnectiion error and launch error
  void CloseConnectionOrLaunchErrorNotifications();

 private:
  friend class EcheAppNotificationControllerTest;

  virtual void LaunchSettings();
  virtual void LaunchTryAgain();
  virtual void LaunchNetworkSettings();

  // Displays the notification to the user.
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification);

  const AccountId account_id_;
  base::RepeatingClosure relaunch_callback_;
  base::WeakPtrFactory<EcheAppNotificationController> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_APP_NOTIFICATION_CONTROLLER_H_
