// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_

#include <list>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace message_center {
class MessagePopupView;
}  // namespace message_center

namespace ui {
class Event;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AutozoomToastController;
class AshMessagePopupCollection;
class CameraMicTrayItemView;
class ChannelIndicatorView;
class CurrentLocaleView;
class ImeModeView;
class ManagedDeviceTrayItemView;
class NetworkTrayView;
class NotificationGroupingController;
class NotificationIconsController;
class PrivacyIndicatorsTrayItemView;
class PrivacyScreenToastController;
class Shelf;
class TrayBubbleView;
class TrayItemView;
class TimeTrayItemView;
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
      public UnifiedSystemTrayController::Observer,
      public TabletModeObserver,
      public message_center::MessageCenterObserver {
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

  // Callback called when this is pressed.
  void OnButtonPressed(const ui::Event& event);

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

  // Shows main bubble with display settings detailed view.
  void ShowDisplayDetailedViewBubble();

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

  // Called by `UnifiedSystemTrayBubble` when it is destroyed with the calendar
  // view in the foreground.
  void NotifyLeavingCalendarView();

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
  void OnAnyBubbleVisibilityChanged(views::Widget* bubble_widget,
                                    bool visible) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // UnifiedSystemTrayController::Observer:
  void OnOpeningCalendarView() override;
  void OnTransitioningFromCalendarToMainView() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // message_center::MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // Gets called when an action is performed on the `DateTray`.
  void OnDateTrayActionPerformed(const ui::Event& event);

  // Whether the bubble is currently showing the calendar view.
  bool IsShowingCalendarView() const;

  // Returns whether the channel indicator should be shown.
  bool ShouldChannelIndicatorBeShown() const;

  std::u16string GetAccessibleNameForQuickSettingsBubble();

  AshMessagePopupCollection* GetMessagePopupCollection();

  NotificationGroupingController* GetNotificationGroupingController();

  scoped_refptr<UnifiedSystemTrayModel> model() { return model_; }
  UnifiedSystemTrayBubble* bubble() { return bubble_.get(); }

  UnifiedMessageCenterBubble* message_center_bubble() {
    return message_center_bubble_.get();
  }

  PrivacyIndicatorsTrayItemView* privacy_indicators_view() {
    return privacy_indicators_view_;
  }

  ChannelIndicatorView* channel_indicator_view() {
    return channel_indicator_view_;
  }

  UnifiedSliderBubbleController* slider_bubble_controller() {
    return slider_bubble_controller_.get();
  }

  CameraMicTrayItemView* camera_view() { return camera_view_; }

  CameraMicTrayItemView* mic_view() { return mic_view_; }

 private:
  static const base::TimeDelta kNotificationCountUpdateDelay;

  friend class NotificationCounterViewTest;
  friend class NotificationGroupingControllerTest;
  friend class NotificationIconsControllerTest;
  friend class SystemTrayTestApi;
  friend class UnifiedSystemTrayTest;

  // Private class implements `MessageCenterUiDelegate`.
  class UiDelegate;

  // Forwarded from `UiDelegate`.
  void ShowBubbleInternal();
  void HideBubbleInternal();
  void UpdateNotificationInternal();
  void UpdateNotificationAfterDelay();

  // Forwarded to `UiDelegate`.
  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id);

  // Adds the tray item to the the unified system tray container. An unowned
  // pointer is stored in `tray_items_`.
  template <typename T>
  T* AddTrayItemToContainer(std::unique_ptr<T> tray_item_view);

  // Destroys the `bubble_` and the `message_center_bubble_`, also handles
  // removing bubble related observers.
  void DestroyBubbles();

  std::unique_ptr<UiDelegate> ui_delegate_;

  std::unique_ptr<UnifiedSystemTrayBubble> bubble_;

  std::unique_ptr<UnifiedMessageCenterBubble> message_center_bubble_;

  // Model class that stores `UnifiedSystemTray`'s UI specific variables.
  scoped_refptr<UnifiedSystemTrayModel> model_;

  const std::unique_ptr<UnifiedSliderBubbleController>
      slider_bubble_controller_;

  const std::unique_ptr<PrivacyScreenToastController>
      privacy_screen_toast_controller_;

  std::unique_ptr<AutozoomToastController> autozoom_toast_controller_;

  // Manages showing notification icons in the tray.
  const std::unique_ptr<NotificationIconsController>
      notification_icons_controller_;

  // Owned by the views hierarchy.
  CurrentLocaleView* current_locale_view_ = nullptr;
  ImeModeView* ime_mode_view_ = nullptr;
  ManagedDeviceTrayItemView* managed_device_view_ = nullptr;
  CameraMicTrayItemView* camera_view_ = nullptr;
  CameraMicTrayItemView* mic_view_ = nullptr;
  TimeTrayItemView* time_view_ = nullptr;
  PrivacyIndicatorsTrayItemView* privacy_indicators_view_ = nullptr;

  NetworkTrayView* network_tray_view_ = nullptr;
  ChannelIndicatorView* channel_indicator_view_ = nullptr;

  // Contains all tray items views added to tray_container().
  std::list<TrayItemView*> tray_items_;

  base::OneShotTimer timer_;

  bool first_interaction_recorded_ = false;

  base::ObserverList<Observer> observers_;

  // Records time the QS bubble was shown. Used for metrics.
  base::TimeTicks time_opened_;

  base::WeakPtrFactory<UnifiedSystemTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
