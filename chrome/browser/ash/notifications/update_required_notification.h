// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_REQUIRED_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_REQUIRED_NOTIFICATION_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

// UpdateRequiredNotification manages in-session notifications informing the
// user that update is required as per admin policy but it cannot be initiated
// either due to network limitations or because of the device having reached its
// end-of-life state.
class UpdateRequiredNotification : public message_center::NotificationObserver {
 public:
  UpdateRequiredNotification();
  UpdateRequiredNotification(const UpdateRequiredNotification&) = delete;
  UpdateRequiredNotification& operator=(const UpdateRequiredNotification&) =
      delete;

  virtual ~UpdateRequiredNotification();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  // Collects notification data like title, body, button text, priority on the
  // basis of |type| and |warning_time|. Sets the |button_click_callback| to be
  // invoked when the notification button is clicked and |close_callback| to be
  // invoked when the notification is closed.
  void Show(policy::MinimumVersionPolicyHandler::NotificationType type,
            base::TimeDelta warning_time,
            const std::string& domain_name,
            const std::u16string& device_type,
            base::OnceClosure button_click_callback,
            base::OnceClosure close_callback);

  void Hide();

 private:
  // Creates and displays a new notification.
  void DisplayNotification(
      const std::u16string& title,
      const std::u16string& message,
      const std::u16string& button_text,
      const message_center::SystemNotificationWarningLevel color_type,
      const message_center::NotificationPriority priority);

  // Callback to be invoked when the notification button is clicked.
  base::OnceClosure notification_button_click_callback_;

  // Callback to be invoked when the notification is closed.
  base::OnceClosure notification_close_callback_;

  base::WeakPtrFactory<UpdateRequiredNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_UPDATE_REQUIRED_NOTIFICATION_H_
