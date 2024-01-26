// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_NOTIFICATION_CONTROLLER_H_

#include <optional>

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

namespace ash {
namespace eche_app {

// Controller class to show notifications.
class EcheAppNotificationController {
 public:
  explicit EcheAppNotificationController(
      Profile* profile,
      const base::RepeatingCallback<void(Profile*)>& relaunch_callback);
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
      absl::variant<LaunchAppHelper::NotificationInfo::NotificationType,
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

  raw_ptr<Profile, DanglingUntriaged> profile_;
  base::RepeatingCallback<void(Profile*)> relaunch_callback_;
  base::WeakPtrFactory<EcheAppNotificationController> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_NOTIFICATION_CONTROLLER_H_
