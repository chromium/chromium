// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_DEPRECATION_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_DEPRECATION_NOTIFICATION_CONTROLLER_H_

#include "ui/message_center/public/cpp/notification_delegate.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

// Manages showing notifications for deprecated event rewrites.
// TODO(crbug.com/1179893): Remove this class after the transition period.
class DeprecationNotificationController {
 public:
  explicit DeprecationNotificationController(
      message_center::MessageCenter* message_center);
  DeprecationNotificationController(const DeprecationNotificationController&) =
      delete;
  DeprecationNotificationController& operator=(
      const DeprecationNotificationController&) = delete;
  virtual ~DeprecationNotificationController();

  // Call to inform the notification controller that Alt-Click was
  // deprecated. Returns true if a notification was displayed. The notification
  // will only be shown once.
  bool NotifyDeprecatedRightClickRewrite();

 private:
  // Show a shortcut deprecation notification using the localized |message_id|
  // as the body. This id must contain a single replacement field that
  // corresponds to the name of the Search/Launcher key.
  void ShowNotificationFromIdWithLauncherKey(const std::string& id,
                                             int message_id);

  // Show a shortcut deprecation notification with the given |message_body|.
  void ShowNotification(const std::string& id,
                        const base::string16& message_body);

  // Used to only show the notification once per user session.
  bool show_right_click_notification_ = true;

  // MessageCenter for adding notifications.
  message_center::MessageCenter* const message_center_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_DEPRECATION_NOTIFICATION_CONTROLLER_H_
