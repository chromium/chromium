// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

namespace tray {
class TimeTrayItemView;
}  // namespace tray

class ImeModeView;
class ManagedDeviceView;
class NotificationCounterView;
class QuietModeView;
class UnifiedSliderBubbleController;
class UnifiedSystemTrayBubble;
class UnifiedSystemTrayModel;

// UnifiedSystemTray is system menu of Chromium OS, which is typically
// accessible from the button on the right bottom of the screen (Status Area).
// The button shows multiple icons on it to indicate system status.
// UnifiedSystemTrayBubble is the actual menu bubble shown on top of it when the
// button is clicked.
//
// UnifiedSystemTray is the view class of that button. It creates and owns
// UnifiedSystemTrayBubble when it is clicked.
//
// UnifiedSystemTray is alternative implementation of SystemTray that is going
// to replace the original one. Eventually, SystemTray will be removed.
class ASH_EXPORT UnifiedSystemTray : public TrayBackgroundView {
 public:
  explicit UnifiedSystemTray(Shelf* shelf);
  ~UnifiedSystemTray() override;

  // True if the bubble is shown. It does not include slider bubbles, and when
  // they're shown it still returns false.
  bool IsBubbleShown() const;

  // True if a slider bubble e.g. volume slider triggered by keyboard
  // accelerator is shown.
  bool IsSliderBubbleShown() const;

  // True if the bubble is active.
  bool IsBubbleActive() const;

  // Activates the system tray bubble.
  void ActivateBubble();

  // Ensure the system tray bubble is expanded.
  void EnsureBubbleExpanded();

  // Shows volume slider bubble shown at the right bottom of screen. The bubble
  // is same as one shown when volume buttons on keyboard are pressed.
  void ShowVolumeSliderBubble();

  // Shows main bubble with audio settings detailed view.
  void ShowAudioDetailedViewBubble();

  // Return the bounds of the bubble in the screen.
  gfx::Rect GetBubbleBoundsInScreen() const;

  // Updates when the login status of the system changes.
  void UpdateAfterLoginStatusChange();

  // Enable / disable UnifiedSystemTray button in status area. If the bubble is
  // open when disabling, also close it.
  void SetTrayEnabled(bool enabled);

  // Sets the height of the system tray bubble from the edge of the work area
  // so that the notification popups don't overlap with the tray. Pass 0 if no
  // bubble is shown.
  void SetTrayBubbleHeight(int height);

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

  UnifiedSystemTrayModel* model() { return model_.get(); }

 private:
  const static base::TimeDelta kNotificationCountUpdateDelay;

  friend class UnifiedSystemTrayTest;
  friend class UnifiedSystemTrayTestApi;

  // Private class implements MessageCenterUiDelegate.
  class UiDelegate;

  // Private class implements TrayNetworkStateObserver::Delegate.
  class NetworkStateDelegate;

  // Forwarded from UiDelegate.
  void ShowBubbleInternal(bool show_by_click);
  void HideBubbleInternal();
  void UpdateNotificationInternal();
  void UpdateNotificationAfterDelay();

  const std::unique_ptr<UiDelegate> ui_delegate_;

  std::unique_ptr<NetworkStateDelegate> network_state_delegate_;

  std::unique_ptr<UnifiedSystemTrayBubble> bubble_;

  // Model class that stores UnifiedSystemTray's UI specific variables.
  const std::unique_ptr<UnifiedSystemTrayModel> model_;

  const std::unique_ptr<UnifiedSliderBubbleController>
      slider_bubble_controller_;

  ImeModeView* const ime_mode_view_;
  ManagedDeviceView* const managed_device_view_;
  NotificationCounterView* const notification_counter_item_;
  QuietModeView* const quiet_mode_view_;
  tray::TimeTrayItemView* const time_view_;

  ui::Layer* ink_drop_layer_ = nullptr;
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
