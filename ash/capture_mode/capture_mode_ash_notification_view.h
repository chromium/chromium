// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_ASH_NOTIFICATION_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_ASH_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ui/views/view_observer.h"

namespace ash {

// A customized notification view for capture mode that adjusts the capture
// notification by either showing a banner on top of the notification image for
// image captures, or a play icon on top of the video thumbnail.
class ASH_EXPORT CaptureModeAshNotificationView : public AshNotificationView,
                                                  public views::ViewObserver {
 public:
  CaptureModeAshNotificationView(
      const message_center::Notification& notification,
      CaptureModeType capture_type,
      bool shown_in_popup);
  CaptureModeAshNotificationView(const CaptureModeAshNotificationView&) =
      delete;
  CaptureModeAshNotificationView& operator=(
      const CaptureModeAshNotificationView&) = delete;
  ~CaptureModeAshNotificationView() override;

  // Creates the custom capture mode notification for image capture
  // notifications. There is a banner on top of the image area of the
  // notification to indicate the image has been copied to clipboard.
  static std::unique_ptr<message_center::MessageView> CreateForImage(
      const message_center::Notification& notification,
      bool shown_in_popup);

  // Creates the custom capture mode notification for video capture
  // notifications. There is a superimposed "play" icon on top of the video
  // thumbnail image.
  static std::unique_ptr<message_center::MessageView> CreateForVideo(
      const message_center::Notification& notification,
      bool shown_in_popup);

  // message_center::NotificationView:
  void Layout() override;

  // views::ViewObserver:
  void OnViewVisibilityChanged(View* observed_view,
                               View* starting_view) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  void CreateExtraView();

  // The type of capture this notification was created for.
  const CaptureModeType capture_type_;

  // The extra view created on top of the notification image. This will be a
  // banner clarifying that the image was copied to the clipboard in case of
  // image capture, or a superimposed "play" icon on top of the video thumbnail
  // image.
  // Owned by the view hierarchy.
  views::View* extra_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_ASH_NOTIFICATION_VIEW_H_
