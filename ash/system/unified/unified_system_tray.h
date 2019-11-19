// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace message_center {
class MessagePopupView;
}  // namespace message_center

namespace ash {

namespace tray {
class TimeTrayItemView;
}  // namespace tray

class CurrentLocaleView;
class ImeModeView;
class ManagedDeviceTrayItemView;
class NotificationCounterView;
class QuietModeView;
class UnifiedSliderBubbleController;
class UnifiedSystemTrayBubble;
class UnifiedSystemTrayModel;
class UnifiedMessageCenterBubble;

// The UnifiedSystemTray is the system menu of Chromium OS, which is a clickable
// rounded rectangle typically located on the bottom right corner of the screen,
// (called the Status Area). The system tray shows multiple icons on it to
// indicate system status (e.g. time, power, etc.).
//
// Note that the Status Area refers to the parent container of the
// UnifiedSystemTray, which also includes other "trays" such as the ImeMenuTray,
// SelectToSpeakTray, VirtualKeyboardTray, etc.
//
// UnifiedSystemTrayBubble is the actual menu bubble shown above the system tray
// after the user clicks on it. The UnifiedSystemTrayBubble is created and owned
// by this class.
class ASH_EXPORT UnifiedSystemTray : public TrayBackgroundView,
                                     public ShelfConfig::Observer {
 public:
  explicit UnifiedSystemTray(Shelf* shelf);
  ~UnifiedSystemTray() override;

  // True if the bubble is shown. It does not include slider bubbles, and when
  // they're shown it still returns false.
  bool IsBubbleShown() const;

  // True if a slider bubble e.g. volume slider triggered by keyboard
  // accelerator is shown.
  bool IsSliderBubbleShown() const;

  // True if the bubble containing notifications is visible..
  bool IsMessageCenterBubbleShown() const;

  // True if the bubble is active.
  bool IsBubbleActive() const;

  // Activates the system tray bubble.
  void ActivateBubble();

  // Collapse the message center bubble.
  void CollapseMessageCenter();

  // Expand the message center bubble.
  void ExpandMessageCenter();

  // Ensure the quick settings bubble is collapsed.
  void EnsureQuickSettingsCollapsed();

  // Ensure the system tray bubble is expanded.
  void EnsureBubbleExpanded();

  // Shows volume slider bubble shown at the right bottom of screen. The bubble
  // is same as one shown when volume buttons on keyboard are pressed.
  void ShowVolumeSliderBubble();

  // Shows main bubble with audio settings detailed view.
  void ShowAudioDetailedViewBubble();

  // Shows main bubble with network settings detailed view.
  void ShowNetworkDetailedViewBubble(bool show_by_click);

  // Return the bounds of the bubble in the screen.
  gfx::Rect GetBubbleBoundsInScreen() const;

  // Updates when the login status of the system changes.
  void UpdateAfterLoginStatusChange();

  // Enable / disable UnifiedSystemTray button in status area. If the bubble is
  // open when disabling, also close it.
  void SetTrayEnabled(bool enabled);

  // Set the target notification, which is visible in the viewport when the
  // message center opens.
  void SetTargetNotification(const std::string& notification_id);

  // Sets the height of the system tray bubble from the edge of the work area
  // so that the notification popups don't overlap with the tray. Pass 0 if no
  // bubble is shown.
  void SetTrayBubbleHeight(int height);

  // Focus the first notification in the message center.
  void FocusFirstNotification();

  bool FocusMessageCenter(bool reverse);

  bool FocusQuickSettings(bool reverse);

  // TrayBackgroundView:
  bool PerformAction(const ui::Event& event) override;
  void ShowBubble(bool show_by_click) override;
  void CloseBubble() override;
  base::string16 GetAccessibleNameForBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void UpdateAfterShelfAlignmentChange() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  const char* GetClassName() const override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  UnifiedSystemTrayModel* model() { return model_.get(); }
  UnifiedSystemTrayBubble* bubble() { return bubble_.get(); }

  UnifiedMessageCenterBubble* message_center_bubble() {
    return message_center_bubble_.get();
  }

 private:
  static const base::TimeDelta kNotificationCountUpdateDelay;

  friend class UnifiedSystemTrayTest;
  friend class UnifiedSystemTrayTestApi;

  // Private class implements MessageCenterUiDelegate.
  class UiDelegate;

  // Forwarded from UiDelegate.
  void ShowBubbleInternal(bool show_by_click);
  void HideBubbleInternal();
  void UpdateNotificationInternal();
  void UpdateNotificationAfterDelay();

  // Forwarded to UiDelegate.
  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id);

  const std::unique_ptr<UiDelegate> ui_delegate_;

  std::unique_ptr<UnifiedSystemTrayBubble> bubble_;

  std::unique_ptr<UnifiedMessageCenterBubble> message_center_bubble_;

  // Model class that stores UnifiedSystemTray's UI specific variables.
  const std::unique_ptr<UnifiedSystemTrayModel> model_;

  const std::unique_ptr<UnifiedSliderBubbleController>
      slider_bubble_controller_;

  CurrentLocaleView* const current_locale_view_;
  ImeModeView* const ime_mode_view_;
  ManagedDeviceTrayItemView* const managed_device_view_;
  NotificationCounterView* const notification_counter_item_;
  QuietModeView* const quiet_mode_view_;
  tray::TimeTrayItemView* const time_view_;

  ui::Layer* ink_drop_layer_ = nullptr;
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
