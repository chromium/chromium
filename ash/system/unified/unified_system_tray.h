// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_

#include <list>
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
class NetworkTrayView;
class TimeTrayItemView;
}  // namespace tray

class CurrentLocaleView;
class ImeModeView;
class ManagedDeviceTrayItemView;
class NotificationCounterView;
class QuietModeView;
class PrivacyScreenToastController;
class TrayItemView;
class UnifiedSliderBubbleController;
class UnifiedSystemTrayBubble;
class UnifiedSystemTrayModel;
class UnifiedMessageCenterBubble;
class CameraMicTrayItemView;

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

  // Closes all non-system tray bubbles (e.g. volume/brightness, and toasts) if
  // any are shown.
  void CloseSecondaryBubbles();

  // Activates the system tray bubble.
  void ActivateBubble();

  // Collapse the message center bubble.
  void CollapseMessageCenter();

  // Expand the message center bubble.
  void ExpandMessageCenter();

  // Ensure the quick settings bubble is collapsed.
  void EnsureQuickSettingsCollapsed(bool animate);

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

  // Transfer focus to the message center bubble.
  bool FocusMessageCenter(bool reverse);

  // Transfer focus to the quick settings bubble.
  bool FocusQuickSettings(bool reverse);

  // Returns true if the user manually expanded the quick settings.
  bool IsQuickSettingsExplicitlyExpanded() const;

  // This enum is for the ChromeOS.SystemTray.FirstInteraction UMA histogram and
  // should be kept in sync.
  enum class FirstInteractionType {
    kQuickSettings = 0,
    kMessageCenter = 1,
    kMaxValue = kMessageCenter,
  };

  // Records a metric of the first interaction with the tray bubble, i.e.
  // whether it was a click/tap on the message center or quick settings.
  void MaybeRecordFirstInteraction(FirstInteractionType type);

  // TrayBackgroundView:
  bool PerformAction(const ui::Event& event) override;
  void ShowBubble(bool show_by_click) override;
  void CloseBubble() override;
  base::string16 GetAccessibleNameForBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void UpdateLayout() override;
  void UpdateAfterLoginStatusChange() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  const char* GetClassName() const override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  base::string16 GetAccessibleNameForQuickSettingsBubble();

  UnifiedSystemTrayModel* model() { return model_.get(); }
  UnifiedSystemTrayBubble* bubble() { return bubble_.get(); }

  UnifiedMessageCenterBubble* message_center_bubble() {
    return message_center_bubble_.get();
  }

 private:
  static const base::TimeDelta kNotificationCountUpdateDelay;

  friend class SystemTrayTestApi;
  friend class UnifiedSystemTrayTest;

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

  // Adds the tray item to the the unified system tray container.
  // The container takes the ownership of |tray_item|.
  void AddTrayItemToContainer(TrayItemView* tray_item);

  const std::unique_ptr<UiDelegate> ui_delegate_;

  std::unique_ptr<UnifiedSystemTrayBubble> bubble_;

  std::unique_ptr<UnifiedMessageCenterBubble> message_center_bubble_;

  // Model class that stores UnifiedSystemTray's UI specific variables.
  const std::unique_ptr<UnifiedSystemTrayModel> model_;

  const std::unique_ptr<UnifiedSliderBubbleController>
      slider_bubble_controller_;

  const std::unique_ptr<PrivacyScreenToastController>
      privacy_screen_toast_controller_;

  CurrentLocaleView* const current_locale_view_;
  ImeModeView* const ime_mode_view_;
  ManagedDeviceTrayItemView* const managed_device_view_;
  CameraMicTrayItemView* const camera_view_;
  CameraMicTrayItemView* const mic_view_;
  NotificationCounterView* const notification_counter_item_;
  QuietModeView* const quiet_mode_view_;
  tray::TimeTrayItemView* const time_view_;

  tray::NetworkTrayView* network_tray_view_ = nullptr;

  // Contains all tray items views added to tray_container().
  std::list<TrayItemView*> tray_items_;

  base::OneShotTimer timer_;

  bool first_interaction_recorded_ = false;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
