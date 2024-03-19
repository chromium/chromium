// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_ACTIONS_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_ACTIONS_VIEW_H_

#include <ash/ash_export.h>
#include <string>

#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace message_center {
class Notification;
}

namespace ui {
class KeyEvent;
}

namespace views {
class FlexLayoutView;
class Textfield;
}  // namespace views

namespace ash {

class SystemTextfield;
class IconButton;

// Displays Notification action buttons and inline reply and forwards responses
// to `message_center::MessageCenter`.
class ASH_EXPORT NotificationActionsView : public views::View,
                                           views::TextfieldController {
  METADATA_HEADER(NotificationActionsView, views::View)

 public:
  NotificationActionsView();
  NotificationActionsView(const NotificationActionsView&) = delete;
  NotificationActionsView& operator=(const NotificationActionsView&) = delete;
  ~NotificationActionsView() override;

  // Updates relevant views with data from the provided `notification`.
  void UpdateWithNotification(const message_center::Notification& notification);

  // Updates the visibility of the `inline_reply_container` and
  // `buttons_container_` and sets up the `send_reply_callback_` after a reply
  // button is pressed.
  void ReplyButtonPressed(const std::string notification_id,
                          const int button_index,
                          const std::u16string placeholder);

  // Sends an input response when the `send_button_` is pressed.
  void SendButtonPressed();

  // Updates the view for the provided expanded state.
  void SetExpanded(bool expanded);

 private:
  friend class NotificationActionsViewTest;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnAfterUserAction(views::Textfield* sender) override;

  // Sends the input reply contained in `textfield_` to
  // `message_center::MessageCenter`.
  void SendReply(const std::string& notification_id, const int button_index);

  // Animates the view to a collapsed state.
  void AnimateCollapse();

  // Animates the view to an expanded state.
  void AnimateExpand();

  // Color used for buttons in this view.
  SkColor button_and_icon_background_color_;

  // Owned by the views hierarchy
  base::RepeatingCallback<void()> send_reply_callback_;

  raw_ptr<views::FlexLayoutView> buttons_container_ = nullptr;
  raw_ptr<views::FlexLayoutView> inline_reply_container_ = nullptr;
  raw_ptr<SystemTextfield> textfield_ = nullptr;
  raw_ptr<IconButton> send_button_ = nullptr;

  base::WeakPtrFactory<NotificationActionsView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFICATION_ACTIONS_VIEW_H_
