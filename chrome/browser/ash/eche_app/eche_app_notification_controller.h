// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_NOTIFICATION_CONTROLLER_H_

#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace ash {
namespace eche_app {

extern const char kEcheAppScreenLockNotifierId[];
extern const char kEcheAppRetryConnectionNotifierId[];
extern const char kEcheAppInactivityNotifierId[];
extern const char kEcheAppFromWebWithoudButtonNotifierId[];
extern const char kEcheAppDisabledByPhoneNotifierId[];
extern const char kEcheAppLearnMoreUrl[];
extern const char kEcheAppHelpUrl[];

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
  void ShowScreenLockNotification(const absl::optional<std::u16string>& title);
  // Shows the notification which was generated from WebUI and carry title and
  // message.
  void ShowNotificationFromWebUI(
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      absl::variant<LaunchAppHelper::NotificationInfo::NotificationType,
                    mojom::WebNotificationType> type);

  // Shows the notification when apps streaming settings is disabled on the
  // phone.
  void ShowDisabledByPhoneNotification(
      const absl::optional<std::u16string>& title);

  // Close the notifiication according to id
  void CloseNotification(const std::string& notification_id);

  // Close the notifiications about coonnectiion error and launch error
  void CloseConnectionOrLaunchErrorNotifications();

 protected:
  // Exposed for testing.
  virtual void LaunchSettings();
  virtual void LaunchLearnMore();
  virtual void LaunchTryAgain();
  virtual void LaunchHelp();

 private:
  // NotificationDelegate implementation for handling click events.
  class NotificationDelegate : public message_center::NotificationDelegate {
   public:
    NotificationDelegate(const std::string& notification_id,
                         const base::WeakPtr<EcheAppNotificationController>&
                             notification_controller);

    NotificationDelegate(const NotificationDelegate&) = delete;
    NotificationDelegate& operator=(const NotificationDelegate&) = delete;

    // message_center::NotificationDelegate:
    void Click(const absl::optional<int>& button_index,
               const absl::optional<std::u16string>& reply) override;

   private:
    ~NotificationDelegate() override;

    std::string notification_id_;
    base::WeakPtr<EcheAppNotificationController> notification_controller_;
  };

  // Displays the notification to the user.
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification);

  Profile* profile_;
  base::RepeatingCallback<void(Profile*)> relaunch_callback_;
  base::WeakPtrFactory<EcheAppNotificationController> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_NOTIFICATION_CONTROLLER_H_
