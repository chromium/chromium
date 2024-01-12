// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_DEPRECATION_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_DEPRECATION_NOTIFICATION_CONTROLLER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/keycodes/keyboard_codes.h"
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
  // will only be shown once per user session.
  bool NotifyDeprecatedRightClickRewrite();

  // Call to inform the notification controller that a legacy six-pack (PageUp,
  // PageDown, Insert, Delete, Home, End) key rewrite (eg. Alt+Up -> PageUp)
  // was deprecated. The |key_code| is the key that would have been generated
  // by the rewrite. The notification will only be shown once per user session.
  bool NotifyDeprecatedSixPackKeyRewrite(ui::KeyboardCode key_code);

  // Reset the state for which notifications have been seen before.
  void ResetStateForTesting();

 private:
  // Show a shortcut deprecation notification using the localized |message_id|
  // as the body. This id must contain a single replacement field that
  // corresponds to the name of the Search/Launcher key.
  void ShowNotificationFromIdWithLauncherKey(const std::string& id,
                                             int message_id);

  // Show a shortcut deprecation notification with the given |message_body|.
  void ShowNotification(const std::string& id,
                        const std::u16string& message_body);

  // Returns whether to show the deprecation notice for |key_code|.
  bool ShouldShowSixPackKeyDeprecationNotification(ui::KeyboardCode key_code);

  // Records that a notification was shown to decide whether to show it again
  // in future.
  void RecordSixPackKeyDeprecationNotificationShown(ui::KeyboardCode key_code);

  // Return the localization message id for |key_code| deprecation.
  int GetSixPackKeyDeprecationMessageId(ui::KeyboardCode key_code);

  // Used to only show the right click notification once per user session.
  bool right_click_notification_shown_ = false;

  // Used to only show the key rewrite notifications once per user session.
  base::flat_set<ui::KeyboardCode> shown_key_notifications_;

  // MessageCenter for adding notifications.
  const raw_ptr<message_center::MessageCenter, DanglingUntriaged>
      message_center_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_DEPRECATION_NOTIFICATION_CONTROLLER_H_
