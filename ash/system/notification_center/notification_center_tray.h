// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class Shelf;
class TrayBubbleView;

// A button in the tray which displays the number of currently available
// notifications along with icons for pinned notifications. Clicking this button
// opens a bubble with a scrollable list of all current notifications.
class ASH_EXPORT NotificationCenterTray : public TrayBackgroundView {
 public:
  METADATA_HEADER(NotificationCenterTray);

  explicit NotificationCenterTray(Shelf* shelf);
  NotificationCenterTray(const NotificationCenterTray&) = delete;
  NotificationCenterTray& operator=(const NotificationCenterTray&) = delete;
  ~NotificationCenterTray() override;

  // TrayBackgroundView:
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void CloseBubble() override;
  void ShowBubble() override;
  void UpdateAfterLoginStatusChange() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;

 private:
  // Manages showing notification icons in the tray.
  const std::unique_ptr<NotificationIconsController>
      notification_icons_controller_;

  // TODO(1311738): Add NotificationCenterBubble.
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_
