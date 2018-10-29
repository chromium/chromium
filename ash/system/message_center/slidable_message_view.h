// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_SLIDABLE_MESSAGE_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_SLIDABLE_MESSAGE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/message_center/notification_swipe_control_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/view.h"

namespace message_center {
class MessageView;
}  // namespace message_center

namespace ash {

class ASH_EXPORT SlidableMessageView
    : public views::View,
      public message_center::MessageView::SlideObserver {
 public:
  explicit SlidableMessageView(message_center::MessageView* message_view);
  ~SlidableMessageView() override;

  message_center::MessageView* GetMessageView() const { return message_view_; }
  static SlidableMessageView* GetFromMessageView(
      message_center::MessageView* message_view);

  // MessageView::SlideObserver
  void OnSlideChanged(const std::string& notification_id) override;

  void SetExpanded(bool expanded) {
    return message_view_->SetExpanded(expanded);
  }

  bool IsManuallyExpandedOrCollapsed() const {
    return message_view_->IsManuallyExpandedOrCollapsed();
  }

  // Updates this view with the new data contained in the notification.
  void UpdateWithNotification(const message_center::Notification& notification);

  std::string notification_id() const {
    return message_view_->notification_id();
  }

  message_center::MessageView::Mode GetMode() const {
    return message_view_->GetMode();
  }

  void CloseSwipeControl();

  // views::View
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

  void UpdateCornerRadius(int top_radius, int bottom_radius);

 private:
  // Owned by views hierarchy.
  message_center::MessageView* const message_view_;
  NotificationSwipeControlView* const control_view_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_SLIDABLE_MESSAGE_VIEW_H_
