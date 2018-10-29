// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UI_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/message_center/message_center_ui_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace message_center {
class MessageCenter;
}

namespace ash {

// Class that observes a MessageCenter and reacts to changes in the list of
// notifications. Manages the popup and message center bubbles. Tells the
// UiDelegate when the tray is changed, as well as when bubbles are shown and
// hidden.
class ASH_EXPORT MessageCenterUiController
    : public message_center::MessageCenterObserver {
 public:
  explicit MessageCenterUiController(MessageCenterUiDelegate* delegate);
  ~MessageCenterUiController() override;

  // Shows or updates the message center bubble and hides the popup bubble. Set
  // |show_by_click| to true if bubble is shown by mouse or gesture click.
  // Returns whether the message center is visible after the call, whether or
  // not it was visible before.
  bool ShowMessageCenterBubble(bool show_by_click);

  // Hides the message center if visible and returns whether the message center
  // was visible before.
  bool HideMessageCenterBubble();

  // Marks the message center as "not visible" (this method will not hide the
  // message center).
  void MarkMessageCenterHidden();

  // Causes an update if the popup bubble is already shown.
  void ShowPopupBubble();

  // Returns whether the popup was visible before.
  bool HidePopupBubble();

  bool message_center_visible() { return message_center_visible_; }
  bool popups_visible() { return popups_visible_; }
  MessageCenterUiDelegate* delegate() { return delegate_; }
  const message_center::MessageCenter* message_center() const {
    return message_center_;
  }
  message_center::MessageCenter* message_center() { return message_center_; }
  void set_hide_on_last_notification(bool hide_on_last_notification) {
    hide_on_last_notification_ = hide_on_last_notification;
  }

  // Overridden from MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnNotificationClicked(
      const std::string& notification_id,
      const base::Optional<int>& button_index,
      const base::Optional<base::string16>& reply) override;
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnQuietModeChanged(bool in_quiet_mode) override;
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override;

 private:
  void OnMessageCenterChanged();
  void NotifyUiControllerChanged();
  void HidePopupBubbleInternal();

  message_center::MessageCenter* const message_center_;
  bool message_center_visible_ = false;
  bool popups_visible_ = false;
  MessageCenterUiDelegate* const delegate_;

  // Set true to hide MessageCenterView when the last notification is dismissed.
  bool hide_on_last_notification_ = true;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterUiController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UI_CONTROLLER_H_
