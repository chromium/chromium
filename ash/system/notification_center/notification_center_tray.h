// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class NotificationListView;
class Shelf;
class TrayBubbleView;

// A button in the tray which displays the number of currently available
// notifications along with icons for pinned notifications. Clicking this button
// opens a bubble with a scrollable list of all current notifications.
class ASH_EXPORT NotificationCenterTray
    : public TrayBackgroundView,
      public message_center::MessageCenterObserver {
 public:
  METADATA_HEADER(NotificationCenterTray);

  explicit NotificationCenterTray(Shelf* shelf);
  NotificationCenterTray(const NotificationCenterTray&) = delete;
  NotificationCenterTray& operator=(const NotificationCenterTray&) = delete;
  ~NotificationCenterTray() override;

  // Called when UnifiedSystemTray's preferred visibility changes.
  void OnSystemTrayVisibilityChanged(bool system_tray_visible);

  NotificationListView* GetNotificationListView();

  // True if the bubble is shown.
  bool IsBubbleShown() const;

  // TrayBackgroundView:
  std::u16string GetAccessibleNameForBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void CloseBubble() override;
  void ShowBubble() override;
  void UpdateAfterLoginStatusChange() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void OnAnyBubbleVisibilityChanged(views::Widget* bubble_widget,
                                    bool visible) override;

 private:
  friend class NotificationCenterTestApi;
  friend class NotificationCounterViewTest;
  friend class NotificationIconsControllerTest;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;

  // Update the visibility of the tray button based on available notifications.
  // If there are no notifications the tray button should be hidden and shown
  // otherwise.
  void UpdateVisibility();

  // Manages showing notification icons in the tray.
  const std::unique_ptr<NotificationIconsController>
      notification_icons_controller_;

  std::unique_ptr<NotificationCenterBubble> bubble_;

  // The notification center tray can only be shown along side the system and
  // date tray. This flag keeps track of the system tray's visibility being set
  // by the status area widget.
  bool system_tray_visible_ = true;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_
