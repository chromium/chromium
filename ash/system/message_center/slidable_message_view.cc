// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/slidable_message_view.h"

#include "ash/system/message_center/notification_swipe_control_view.h"
#include "ui/message_center/public/cpp/features.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

SlidableMessageView::SlidableMessageView(
    message_center::MessageView* message_view)
    : message_view_(message_view),
      control_view_(new NotificationSwipeControlView(message_view)) {
  SetFocusBehavior(views::View::FocusBehavior::NEVER);

  // Draw on its own layer to allow bound animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  control_view_->UpdateCornerRadius(0, 0);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  AddChildView(control_view_);

  message_view_->AddSlideObserver(this);
  AddChildView(message_view);
}

SlidableMessageView::~SlidableMessageView() = default;

void SlidableMessageView::OnSlideChanged(const std::string& notification_id) {
  control_view_->UpdateButtonsVisibility();
}

void SlidableMessageView::CloseSwipeControl() {
  message_view_->CloseSwipeControl();
}

void SlidableMessageView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
  InvalidateLayout();
}

void SlidableMessageView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
  InvalidateLayout();
}

void SlidableMessageView::UpdateWithNotification(
    const message_center::Notification& notification) {
  message_view_->UpdateWithNotification(notification);
}

gfx::Size SlidableMessageView::CalculatePreferredSize() const {
  return message_view_->GetPreferredSize();
}

int SlidableMessageView::GetHeightForWidth(int width) const {
  return message_view_->GetHeightForWidth(width);
}

void SlidableMessageView::UpdateCornerRadius(int top_radius,
                                             int bottom_radius) {
  control_view_->UpdateCornerRadius(top_radius, bottom_radius);
}

// static
SlidableMessageView* SlidableMessageView::GetFromMessageView(
    message_center::MessageView* message_view) {
  DCHECK(message_view);
  DCHECK(message_view->parent());
  DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
            message_view->parent()->GetClassName());
  return static_cast<SlidableMessageView*>(message_view->parent());
}

}  // namespace ash
