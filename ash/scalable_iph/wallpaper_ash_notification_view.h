// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCALABLE_IPH_WALLPAPER_ASH_NOTIFICATION_VIEW_H_
#define ASH_SCALABLE_IPH_WALLPAPER_ASH_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "ash/scalable_iph/scalable_iph_ash_notification_view.h"
#include "base/memory/raw_ptr.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace views {
class View;
}  // namespace views

namespace ash {

class RoundedImageView;

// A customized notification view for scalable IPH that adjusts the notification
// by showing four preview images for wallpaper.
class ASH_EXPORT WallpaperAshNotificationView
    : public ScalableIphAshNotificationView {
  METADATA_HEADER(WallpaperAshNotificationView, ScalableIphAshNotificationView)

 public:
  WallpaperAshNotificationView(const message_center::Notification& notification,
                               bool shown_in_popup);
  WallpaperAshNotificationView(const WallpaperAshNotificationView&) = delete;
  WallpaperAshNotificationView& operator=(const WallpaperAshNotificationView&) =
      delete;
  ~WallpaperAshNotificationView() override;

  // Creates the custom notification with wallpaper preview images.
  static std::unique_ptr<message_center::MessageView> CreateWithPreview(
      const message_center::Notification& notification,
      bool shown_in_popup);

  // AshNotificationView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;

 private:
  friend class WallpaperAshNotificationViewTest;

  void CreatePreview();

  // The preview view created to replace the notification image.
  // Owned by the view hierarchy.
  raw_ptr<views::View> preview_ = nullptr;
  std::vector<raw_ptr<RoundedImageView, VectorExperimental>> image_views_{
      4, nullptr};
};

}  // namespace ash

#endif  // ASH_SCALABLE_IPH_WALLPAPER_ASH_NOTIFICATION_VIEW_H_
