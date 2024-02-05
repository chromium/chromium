// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_ash_notification_view.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

namespace ash {

CaptureModeAshNotificationView::CaptureModeAshNotificationView(
    const message_center::Notification& notification,
    CaptureModeType capture_type,
    bool shown_in_popup)
    : AshNotificationView(notification, shown_in_popup),
      capture_type_(capture_type) {
  UpdateWithNotification(notification);
}

CaptureModeAshNotificationView::~CaptureModeAshNotificationView() = default;

// static
std::unique_ptr<message_center::MessageView>
CaptureModeAshNotificationView::CreateForImage(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<CaptureModeAshNotificationView>(
      notification, CaptureModeType::kImage, shown_in_popup);
}

// static
std::unique_ptr<message_center::MessageView>
CaptureModeAshNotificationView::CreateForVideo(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<CaptureModeAshNotificationView>(
      notification, CaptureModeType::kVideo, shown_in_popup);
}

void CaptureModeAshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  // Re-create a new extra view in all circumstances to make sure that the view
  // is the last child of image container.
  delete extra_view_;
  extra_view_ = nullptr;

  NotificationViewBase::UpdateWithNotification(notification);

  if (!notification.image().IsEmpty())
    CreateExtraView();
}

void CaptureModeAshNotificationView::Layout(PassKey) {
  LayoutSuperclass<AshNotificationView>(this);
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

void CaptureModeAshNotificationView::CreateExtraView() {
  DCHECK(image_container_view());
  DCHECK(!image_container_view()->children().empty());
  DCHECK(!extra_view_);
  extra_view_ = image_container_view()->AddChildView(
      capture_type_ == CaptureModeType::kImage
          ? capture_mode_util::CreateBannerView()
          : capture_mode_util::CreatePlayIconView());
}

BEGIN_METADATA(CaptureModeAshNotificationView)
END_METADATA

}  // namespace ash
