// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_NOTIFICATION_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/message_center/views/notification_view_md.h"
#include "ui/views/view_observer.h"

namespace ash {

// A customized notification view for capture mode that can show a notification
// with a banner on top of the notification image.
class ASH_EXPORT CaptureModeNotificationView
    : public message_center::NotificationViewMD,
      public views::ViewObserver {
 public:
  explicit CaptureModeNotificationView(
      const message_center::Notification& notification);
  CaptureModeNotificationView(const CaptureModeNotificationView&) = delete;
  CaptureModeNotificationView& operator=(const CaptureModeNotificationView&) =
      delete;
  ~CaptureModeNotificationView() override;

  // Creates the custom capture mode notification for image capture
  // notification. There is a banner on top of the image area of the
  // notification to indicate the image has been copied to clipboard.
  static std::unique_ptr<message_center::MessageView> Create(
      const message_center::Notification& notification);

  // message_center::NotificationViewMD:
  void Layout() override;

  // views::ViewObserver:
  void OnChildViewAdded(views::View* observed_view,
                        views::View* child) override;
  void OnChildViewRemoved(views::View* observed_view,
                          views::View* child) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  void CreateBannerView();

  // The banner view that shows a banner string on top of the captured image.
  // Owned by view hierarchy.
  views::View* banner_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_NOTIFICATION_VIEW_H_
