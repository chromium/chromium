// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chromeos/components/proximity_auth/notification_controller.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace chromeos {

class EasyUnlockNotificationController
    : public proximity_auth::NotificationController {
 public:
  explicit EasyUnlockNotificationController(Profile* profile);
  ~EasyUnlockNotificationController() override;

  // proximity_auth::NotificationController:
  void ShowChromebookAddedNotification() override;
  void ShowPairingChangeNotification() override;
  void ShowPairingChangeAppliedNotification(
      const std::string& phone_name) override;

 protected:
  // Exposed for testing.
  virtual void LaunchEasyUnlockSettings();
  virtual void LockScreen();

 private:
  // NotificationDelegate implementation for handling click events.
  class NotificationDelegate : public message_center::NotificationDelegate {
   public:
    NotificationDelegate(const std::string& notification_id,
                         const base::WeakPtr<EasyUnlockNotificationController>&
                             notification_controller);

    // message_center::NotificationDelegate:
    void Click(const base::Optional<int>& button_index,
               const base::Optional<base::string16>& reply) override;

   private:
    ~NotificationDelegate() override;

    std::string notification_id_;
    base::WeakPtr<EasyUnlockNotificationController> notification_controller_;

    DISALLOW_COPY_AND_ASSIGN(NotificationDelegate);
  };

  // Displays the notification to the user.
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification);

  Profile* profile_;

  base::WeakPtrFactory<EasyUnlockNotificationController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_
