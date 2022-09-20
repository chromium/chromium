// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_notification_view.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ui/views/view.h"

namespace ash {

CaptureModeNotificationView::CaptureModeNotificationView(
    const message_center::Notification& notification,
    CaptureModeType capture_type)
    : message_center::NotificationView(notification),
      capture_type_(capture_type) {
  UpdateWithNotification(notification);
}

CaptureModeNotificationView::~CaptureModeNotificationView() = default;

// static
std::unique_ptr<message_center::MessageView>
CaptureModeNotificationView::CreateForImage(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<CaptureModeNotificationView>(notification,
                                                       CaptureModeType::kImage);
}

// static
std::unique_ptr<message_center::MessageView>
CaptureModeNotificationView::CreateForVideo(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<CaptureModeNotificationView>(notification,
                                                       CaptureModeType::kVideo);
}

void CaptureModeNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  // Re-create a new extra view in all circumstances to make sure that the view
  // is the last child of image container.
  delete extra_view_;
  extra_view_ = nullptr;

  NotificationView::UpdateWithNotification(notification);

  if (!notification.image().IsEmpty())
    CreateExtraView();
}

void CaptureModeNotificationView::Layout() {
  message_center::NotificationView::Layout();
  if (!extra_view_)
    return;

  gfx::Rect extra_view_bounds = image_container_view()->GetContentsBounds();

  if (capture_type_ == CaptureModeType::kImage) {
    // The extra view in this case is a banner laid out at the bottom of the
    // image container.
    extra_view_bounds.set_y(extra_view_bounds.bottom() -
                            capture_mode_util::kBannerHeightDip);
    extra_view_bounds.set_height(capture_mode_util::kBannerHeightDip);
  } else {
    DCHECK_EQ(capture_type_, CaptureModeType::kVideo);
    // The extra view in this case is a play icon centered in the view.
    extra_view_bounds.ClampToCenteredSize(capture_mode_util::kPlayIconViewSize);
  }

  extra_view_->SetBoundsRect(extra_view_bounds);
}

void CaptureModeNotificationView::CreateExtraView() {
  DCHECK(image_container_view());
  DCHECK(!image_container_view()->children().empty());
  DCHECK(!extra_view_);
  extra_view_ = image_container_view()->AddChildView(
      capture_type_ == CaptureModeType::kImage
          ? capture_mode_util::CreateBannerView()
          : capture_mode_util::CreatePlayIconView());
}

}  // namespace ash
