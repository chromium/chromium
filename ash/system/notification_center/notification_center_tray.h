// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/notification_grouping_controller.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class NotificationListView;
class NotificationMetricsRecorder;
class PrivacyIndicatorsTrayItemView;
class Shelf;
class TrayBubbleView;

// A button in the tray which displays the number of currently available
// notifications along with icons for pinned notifications. Clicking this button
// opens a bubble with a scrollable list of all current notifications.
class ASH_EXPORT NotificationCenterTray : public TrayBackgroundView,
                                          public TrayItemView::Observer {
  METADATA_HEADER(NotificationCenterTray, TrayBackgroundView)

 public:
  // Inherit from this class to be notified of events that happen for a specific
  // `NotificationCenterTray`.
  class Observer : public base::CheckedObserver {
   public:
    // Called when all `TrayItemView`s are done being added to this
    // `NotificationCenterTray`.
    virtual void OnAllTrayItemsAdded() = 0;
  };

  explicit NotificationCenterTray(Shelf* shelf);
  NotificationCenterTray(const NotificationCenterTray&) = delete;
  NotificationCenterTray& operator=(const NotificationCenterTray&) = delete;
  ~NotificationCenterTray() override;

  void AddNotificationCenterTrayObserver(Observer* observer);
  void RemoveNotificationCenterTrayObserver(Observer* observer);

  // Called when UnifiedSystemTray's preferred visibility changes.
  void OnSystemTrayVisibilityChanged(bool system_tray_visible);

  // Callback called when this TrayBackgroundView is pressed.
  void OnTrayButtonPressed();

  NotificationListView* GetNotificationListView();

  // True if the bubble is shown.
  bool IsBubbleShown() const;

  // Update the visibility of the tray button based on available notifications.
  // If there are no notifications the tray button should be hidden and shown
  // otherwise.
  void UpdateVisibility();

  // TrayBackgroundView:
  void Initialize() override;
  std::u16string GetAccessibleNameForBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  void UpdateAfterLoginStatusChange() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void UpdateLayout() override;

  // ash::TrayItemView::Observer:
  void OnTrayItemVisibilityAboutToChange(bool target_visibility) override;

  PrivacyIndicatorsTrayItemView* privacy_indicators_view() {
    return privacy_indicators_view_;
  }

  NotificationIconsController* notification_icons_controller() {
    return notification_icons_controller_.get();
  }

  NotificationGroupingController* notification_grouping_controller() {
    return notification_grouping_controller_.get();
  }

  AshMessagePopupCollection* popup_collection() {
    return popup_collection_.get();
  }

  NotificationCenterBubble* bubble() { return bubble_.get(); }

 private:
  friend class NotificationCenterTestApi;
  friend class NotificationCounterViewTest;
  friend class NotificationIconsControllerTest;

  // Manages notification grouping.
  const std::unique_ptr<NotificationGroupingController>
      notification_grouping_controller_;

  // Manages notification popups.
  const std::unique_ptr<AshMessagePopupCollection> popup_collection_;

  // Manages notification metrics.
  const std::unique_ptr<NotificationMetricsRecorder>
      notification_metrics_recorder_;

  // Manages showing notification icons in the tray.
  const std::unique_ptr<NotificationIconsController>
      notification_icons_controller_;

  // Owned by the views hierarchy.
  raw_ptr<PrivacyIndicatorsTrayItemView> privacy_indicators_view_ = nullptr;

  std::unique_ptr<NotificationCenterBubble> bubble_;

  // The notification center tray can only be shown along side the system and
  // date tray. This flag keeps track of the system tray's visibility being set
  // by the status area widget.
  bool system_tray_visible_ = true;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_CENTER_TRAY_H_
