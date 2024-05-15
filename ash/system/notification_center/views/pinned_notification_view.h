// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_PINNED_NOTIFICATION_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_PINNED_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/views/message_view.h"

namespace message_center {
class Notification;
class NotificationControlButtonsView;
}  // namespace message_center

namespace views {
class Label;
}

namespace ash {

class IconButton;
class PillButton;

// Ash Pinned Notification View to be used when the `Ongoing Processes` flag is
// enabled (go/ongoing-processes-spec). This view must have a title and an icon,
// and optionally supports a subtitle, and a pill button or up to two icon
// buttons.
class ASH_EXPORT PinnedNotificationView : public message_center::MessageView {
  METADATA_HEADER(PinnedNotificationView, message_center::MessageView)

 public:
  PinnedNotificationView(const message_center::Notification& notification);
  PinnedNotificationView(const PinnedNotificationView&) = delete;
  PinnedNotificationView& operator=(const PinnedNotificationView&) = delete;
  ~PinnedNotificationView() override;

  // message_center::MessageView:
  void OnFocus() override;
  void OnThemeChanged() override;
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Owned by the views hierarchy.
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> subtitle_label_ = nullptr;
  raw_ptr<IconButton> secondary_button_ = nullptr;
  raw_ptr<IconButton> primary_icon_button_ = nullptr;
  raw_ptr<PillButton> primary_pill_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_PINNED_NOTIFICATION_VIEW_H_
