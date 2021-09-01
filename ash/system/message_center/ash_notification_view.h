// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/controls/button/image_button.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// Customized NotificationView for notification on ChromeOS. This view is used
// to displays all current types of notification on ChromeOS (web, basic, image,
// and list) except custom notification.
class ASH_EXPORT AshNotificationView
    : public message_center::NotificationViewBase {
 public:
  // TODO(crbug/1241983): Add metadata and builder support to this view.
  explicit AshNotificationView(const message_center::Notification& notification,
                               bool shown_in_popup);
  AshNotificationView(const AshNotificationView&) = delete;
  AshNotificationView& operator=(const AshNotificationView&) = delete;
  ~AshNotificationView() override;

  // Toggle the expand state of the notification.
  void ToggleExpand();

  // message_center::NotificationView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  void SetExpanded(bool expanded) override;
  void SetExpandButtonEnabled(bool enabled) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  void SetDrawBackgroundAsActive(bool active) override;
  void OnThemeChanged() override;

 private:
  friend class AshNotificationViewTest;

  // Customized expand button for this notification view.
  class ExpandButton : public views::ImageButton {
   public:
    METADATA_HEADER(ExpandButton);
    explicit ExpandButton(PressedCallback callback);
    ExpandButton(const ExpandButton&) = delete;
    ExpandButton& operator=(const ExpandButton&) = delete;
    ~ExpandButton() override;

    // Change the expanded state. The icon will change.
    void SetExpanded(bool expanded);

    // views::ImageButton:
    gfx::Size CalculatePreferredSize() const override;
    void PaintButtonContents(gfx::Canvas* canvas) override;
    void OnThemeChanged() override;

   private:
    // The expand state of the button.
    bool expanded_ = false;
  };

  // Update the background color with rounded corner.
  void UpdateBackground(int top_radius, int bottom_radius);

  ExpandButton* expand_button_ = nullptr;

  // Corner radius of the notification view.
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  // Cached background color to avoid unnecessary update.
  SkColor background_color_ = SK_ColorTRANSPARENT;

  // Whether this view is shown in a notification popup.
  bool shown_in_popup_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_VIEW_H_