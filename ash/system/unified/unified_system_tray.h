// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_

#include <list>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shell_observer.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace message_center {
class MessagePopupView;
}  // namespace message_center

namespace ash {

class AshMessagePopupCollection;
class CameraMicTrayItemView;
class CurrentLocaleView;
class ImeModeView;
class ManagedDeviceTrayItemView;
class NetworkTrayView;
class NotificationIconsController;
class PrivacyScreenToastController;
class SnoopingProtectionView;
class TimeTrayItemView;
class TrayItemView;
class UnifiedSliderBubbleController;
class UnifiedSystemTrayBubble;
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
class ASH_EXPORT UnifiedSystemTray
    : public TrayBackgroundView,
      public ShelfConfig::Observer,
      public ShellObserver,
      public UnifiedSystemTrayController::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Gets called when showing calendar view.
    virtual void OnOpeningCalendarView() {}

    // Gets called when leaving from the calendar view.
    virtual void OnLeavingCalendarView() {}
  };

  explicit UnifiedSystemTray(Shelf* shelf);

  UnifiedSystemTray(const UnifiedSystemTray&) = delete;
  UnifiedSystemTray& operator=(const UnifiedSystemTray&) = delete;

  ~UnifiedSystemTray() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Adds a padding on top of the vertical clock if there are other visible
  // icons in the tray, removes it if the clock is the only visible icon.
  void MaybeUpdateVerticalClockPadding();

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

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
  void ShowNetworkDetailedViewBubble();

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

  // Transfer focus to the message center bubble. Will focus only on the message
  // center if vox is enabled. Otherwise, will focus on the first element in the
  // message center while honoring the `reverse` attribute that is passed in.
  bool FocusMessageCenter(bool reverse, bool collapse_quick_settings = false);

  // Transfer focus to the quick settings bubble. Will focus only on the quick
  // settings bubble if vox is enabled. Otherwise, will focus on the first
  // element in the quick settings while honoring the `reverse` attribute that
  // is passed in.
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
  void ShowBubble() override;
  void CloseBubble() override;
  std::u16string GetAccessibleNameForBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void UpdateLayout() override;
  void UpdateAfterLoginStatusChange() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  views::Widget* GetBubbleWidget() const override;
  const char* GetClassName() const override;
  absl::optional<AcceleratorAction> GetAcceleratorAction() const override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // UnifiedSystemTrayController::Observer:
  void OnOpeningCalendarView() override;
  void OnTransitioningFromCalendarToMainView() override;

  // Gets called when an action is performed on the `DateTray`.
  void OnDateTrayActionPerformed(const ui::Event& event);

  // Whether the bubble is currently showing the calendar view.
  bool IsShowingCalendarView() const;

  std::u16string GetAccessibleNameForQuickSettingsBubble();

  AshMessagePopupCollection* GetMessagePopupCollection();

  scoped_refptr<UnifiedSystemTrayModel> model() { return model_; }
  UnifiedSystemTrayBubble* bubble() { return bubble_.get(); }

  UnifiedMessageCenterBubble* message_center_bubble() {
    return message_center_bubble_.get();
  }

 private:
  static const base::TimeDelta kNotificationCountUpdateDelay;

  friend class NotificationGroupingControllerTest;
  friend class SystemTrayTestApi;
  friend class UnifiedSystemTrayTest;

  // Private class implements MessageCenterUiDelegate.
  class UiDelegate;

  // Forwarded from UiDelegate.
  void ShowBubbleInternal();
  void HideBubbleInternal();
  void UpdateNotificationInternal();
  void UpdateNotificationAfterDelay();

  // Forwarded to UiDelegate.
  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id);

  // Adds the tray item to the the unified system tray container.
  // The container takes the ownership of |tray_item|.
  void AddTrayItemToContainer(TrayItemView* tray_item);

  // Returns true if there is two or more tray items that are visible.
  bool MoreThanOneVisibleTrayItem() const;

  // Add observed tray item views.
  void AddObservedTrayItem(TrayItemView* tray_item);

  // Destroys the `bubble_` and the `message_center_bubble_`, also handles
  // removing bubble related observers.
  void DestroyBubbles();

  const std::unique_ptr<UiDelegate> ui_delegate_;

  std::unique_ptr<UnifiedSystemTrayBubble> bubble_;

  std::unique_ptr<UnifiedMessageCenterBubble> message_center_bubble_;

  // Model class that stores UnifiedSystemTray's UI specific variables.
  scoped_refptr<UnifiedSystemTrayModel> model_;

  const std::unique_ptr<UnifiedSliderBubbleController>
      slider_bubble_controller_;

  const std::unique_ptr<PrivacyScreenToastController>
      privacy_screen_toast_controller_;

  // Manages showing notification icons in the tray.
  const std::unique_ptr<NotificationIconsController>
      notification_icons_controller_;

  SnoopingProtectionView* const snooping_protection_view_;
  CurrentLocaleView* const current_locale_view_;
  ImeModeView* const ime_mode_view_;
  ManagedDeviceTrayItemView* const managed_device_view_;
  CameraMicTrayItemView* const camera_view_;
  CameraMicTrayItemView* const mic_view_;
  TimeTrayItemView* const time_view_;

  NetworkTrayView* network_tray_view_ = nullptr;

  // Contains all tray items views added to tray_container().
  std::list<TrayItemView*> tray_items_;

  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      tray_items_observations_{this};

  // Padding owned by the view hierarchy used to separate vertical
  // clock from other tray icons.
  views::View* vertical_clock_padding_ = nullptr;

  base::OneShotTimer timer_;

  bool first_interaction_recorded_ = false;

  base::ObserverList<Observer> observers_;

  // Records time the QS bubble was shown. Used for metrics.
  base::TimeTicks time_opened_;

  base::WeakPtrFactory<UnifiedSystemTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
