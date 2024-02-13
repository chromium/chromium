// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_ASH_NOTIFICATION_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_ASH_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "base/memory/raw_ptr.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace views {
class View;
}  // namespace views

namespace ash {

// A customized notification view for capture mode that adjusts the capture
// notification by either showing a banner on top of the notification image for
// image captures, or a play icon on top of the video thumbnail.
class ASH_EXPORT CaptureModeAshNotificationView : public AshNotificationView {
  METADATA_HEADER(CaptureModeAshNotificationView, AshNotificationView)

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

  // AshNotificationView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  void Layout(PassKey) override;

 private:
  void CreateExtraView();

  // The type of capture this notification was created for.
  const CaptureModeType capture_type_;

  // The extra view created on top of the notification image. This will be a
  // banner clarifying that the image was copied to the clipboard in case of
  // image capture, or a superimposed "play" icon on top of the video thumbnail
  // image.
  // Owned by the view hierarchy.
  raw_ptr<views::View, DanglingUntriaged> extra_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_ASH_NOTIFICATION_VIEW_H_
