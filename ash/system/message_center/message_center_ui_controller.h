// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UI_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UI_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/message_center/message_center_ui_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
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
    : public message_center::MessageCenterObserver,
      public SessionObserver {
 public:
  explicit MessageCenterUiController(MessageCenterUiDelegate* delegate);

  MessageCenterUiController(const MessageCenterUiController&) = delete;
  MessageCenterUiController& operator=(const MessageCenterUiController&) =
      delete;

  ~MessageCenterUiController() override;

  // Shows or updates the message center bubble and hides the popup bubble.
  // Returns whether the message center is visible after the call, whether or
  // not it was visible before.
  bool ShowMessageCenterBubble();

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
      const absl::optional<int>& button_index,
      const absl::optional<std::u16string>& reply) override;
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnQuietModeChanged(bool in_quiet_mode) override;
  void OnBlockingStateChanged(
      message_center::NotificationBlocker* blocker) override;
  void OnNotificationPopupShown(const std::string& notification_id,
                                bool mark_notification_as_read) override;
  void OnMessageViewHovered(const std::string& notification_id) override;

  // SessionObserver:
  void OnFirstSessionStarted() override;

 private:
  void OnLoginTimerEnded();
  void OnMessageCenterChanged();
  void NotifyUiControllerChanged();
  void HidePopupBubbleInternal();

  const raw_ptr<message_center::MessageCenter, ExperimentalAsh> message_center_;
  bool message_center_visible_ = false;
  bool popups_visible_ = false;
  const raw_ptr<MessageCenterUiDelegate, ExperimentalAsh> delegate_;

  // Set true to hide MessageCenterView when the last notification is dismissed.
  bool hide_on_last_notification_ = true;

  int notifications_displayed_in_first_minute_count_ = 0;
  base::OneShotTimer login_notification_logging_timer_;

  base::ScopedObservation<SessionController, SessionObserver> session_observer_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UI_CONTROLLER_H_
