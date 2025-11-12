// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_NOTIFICATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace ash {

class SmartLockNotificationController {
 public:
  explicit SmartLockNotificationController(Profile* profile);

  SmartLockNotificationController(const SmartLockNotificationController&) =
      delete;
  SmartLockNotificationController& operator=(
      const SmartLockNotificationController&) = delete;

  virtual ~SmartLockNotificationController();

  // Shows the notification when SmartLock is synced to a new Chromebook.
  virtual void ShowChromebookAddedNotification();

  // Shows the notification when SmartLock is already enabled on a Chromebook,
  // but a different phone is synced as the unlock key.
  virtual void ShowPairingChangeNotification();

  // Shows the notification after password reauth confirming that the new phone
  // should be used for SmartLock from now on.
  virtual void ShowPairingChangeAppliedNotification(
      const std::string& phone_name);

 protected:
  // Exposed for testing.
  virtual void LaunchMultiDeviceSettings();
  virtual void LockScreen();

 private:
  // NotificationDelegate implementation for handling click events.
  class NotificationDelegate : public message_center::NotificationDelegate {
   public:
    NotificationDelegate(const std::string& notification_id,
                         const base::WeakPtr<SmartLockNotificationController>&
                             notification_controller);

    NotificationDelegate(const NotificationDelegate&) = delete;
    NotificationDelegate& operator=(const NotificationDelegate&) = delete;

    // message_center::NotificationDelegate:
    void Click(const std::optional<int>& button_index,
               const std::optional<std::u16string>& reply) override;

   private:
    ~NotificationDelegate() override;

    std::string notification_id_;
    base::WeakPtr<SmartLockNotificationController> notification_controller_;
  };

  // Displays the notification to the user.
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification);

  raw_ptr<Profile, DanglingUntriaged> profile_;

  base::WeakPtrFactory<SmartLockNotificationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_NOTIFICATION_CONTROLLER_H_
