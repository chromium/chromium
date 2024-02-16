// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_VIEW_CONTAINER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_VIEW_CONTAINER_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

// Container view for `MessageView` objects, which are initialized with a
// `message_center::Notification`.
class MessageViewContainer : public views::View {
  METADATA_HEADER(MessageViewContainer, views::View)

 public:
  explicit MessageViewContainer(
      std::unique_ptr<message_center::MessageView> message_view);
  MessageViewContainer(const MessageViewContainer&) = delete;
  MessageViewContainer& operator=(const MessageViewContainer&) = delete;
  ~MessageViewContainer() override = default;

  // Calls `GetHeightForWidth` on the cached `message_view_`.
  int CalculateHeight() const;

  // Updates the corner radius based on if the view is at the top or the bottom
  // of its parent list view. If `force_update` is true, the corner radius and
  // background will be updated even if `is_top` and `is_bottom` have the same
  // value as the stored variables.
  void UpdateBorder(const bool is_top,
                    const bool is_bottom,
                    const bool force_update = false);

  // Gets the `notification_id` stored in `message_view_`.
  const std::string GetNotificationId() const;

  // Forwards call to `UpdateWithNotification` in `message_view_`.
  void UpdateWithNotification(const message_center::Notification& notification);

  // views::View
  gfx::Size CalculatePreferredSize() const override;

  message_center::MessageView* message_view() { return message_view_; }
  const message_center::MessageView* message_view() const {
    return message_view_;
  }

 private:
  // Used to track if this view is at the top or bottom of its parent list view
  // and prevent unnecessary updates.
  bool is_top_ = false;
  bool is_bottom_ = false;

  // Owned by `this`.
  raw_ptr<message_center::MessageView> message_view_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_MESSAGE_VIEW_CONTAINER_H_
