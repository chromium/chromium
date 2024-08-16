// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_

#include <list>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/system/power/tray_power.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ui {
class Event;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

class AutozoomToastController;
class CameraMicTrayItemView;
class ChannelIndicatorView;
class CurrentLocaleView;
class HotspotTrayView;
class ImeModeView;
class ManagedDeviceTrayItemView;
class NetworkTrayView;
class PrivacyScreenToastController;
class Shelf;
class TrayBubbleView;
class TrayItemView;
class TimeTrayItemView;
class UnifiedSliderBubbleController;
class UnifiedSliderView;
class UnifiedSystemTrayBubble;

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
      public display::DisplayObserver {
  METADATA_HEADER(UnifiedSystemTray, TrayBackgroundView)

 public:
  class Observer : public base::CheckedObserver {
   public:
    // Gets called when showing calendar view.
    virtual void OnOpeningCalendarView() {}

    // Gets called when leaving from the calendar view.
    virtual void OnLeavingCalendarView() {}

    // Gets called when a slider bubble is shown or closed.
    virtual void OnSliderBubbleHeightChanged() {}
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

  // Gets the slider view of the slider bubble.
  UnifiedSliderView* GetSliderView() const;

  // True if the bubble is active.
  bool IsBubbleActive() const;

  // Closes all secondary bubbles (e.g. volume/brightness sliders, autozoom,
  // privacy screen toast) if any are shown.
  // TODO(b/279044049): Consider making autozoom and privacy screen slider
  // bubbles, or have them extend a common class with sliders.
  void CloseSecondaryBubbles();

  // Activates the system tray bubble.
  void ActivateBubble();

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

  // Notifies the height of the secondary bubble (e.g. Volume/Brightness
  // sliders, Privacy screen/Autozoom toast) if one is showing so notification
  // popups or toasts won't overlap with it. Pass 0 if no bubble is shown.
  void NotifySecondaryBubbleHeight(int height);

  // Called by `UnifiedSystemTrayBubble` when it is destroyed with the calendar
  // view in the foreground.
  void NotifyLeavingCalendarView();

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
  void CloseBubbleInternal() override;
  std::u16string GetAccessibleNameForBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  void UpdateLayout() override;
  void UpdateAfterLoginStatusChange() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  views::Widget* GetBubbleWidget() const override;
  TrayBubbleView* GetBubbleView() override;
  std::optional<AcceleratorAction> GetAcceleratorAction() const override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // UnifiedSystemTrayController::Observer:
  void OnOpeningCalendarView() override;
  void OnTransitioningFromCalendarToMainView() override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Gets called when an action is performed on the `DateTray`.
  void OnDateTrayActionPerformed(const ui::Event& event);

  // Whether the bubble is currently showing the calendar view.
  bool IsShowingCalendarView() const;

  // Returns whether the channel indicator should be shown.
  bool ShouldChannelIndicatorBeShown() const;

  std::u16string GetAccessibleNameForQuickSettingsBubble();

  scoped_refptr<UnifiedSystemTrayModel> model() { return model_; }
  UnifiedSystemTrayBubble* bubble() { return bubble_.get(); }

  ChannelIndicatorView* channel_indicator_view() {
    return channel_indicator_view_;
  }

  UnifiedSliderBubbleController* slider_bubble_controller() {
    return slider_bubble_controller_.get();
  }

  CameraMicTrayItemView* mic_view() { return mic_view_; }

 private:
  friend class NotificationGroupingControllerTest;
  friend class SystemTrayTestApi;
  friend class UnifiedSystemTrayTest;
  friend class PowerTrayViewTest;
  friend class StatusAreaBatteryPixelTest;

  // Forwarded from `UiDelegate`.
  void ShowBubbleInternal();
  void HideBubbleInternal();

  // Adds the tray item to the the unified system tray container. An unowned
  // pointer is stored in `tray_items_`.
  template <typename T>
  T* AddTrayItemToContainer(std::unique_ptr<T> tray_item_view);

  // Destroys the `bubble_`, also handles
  // removing bubble related observers.
  void DestroyBubble();

  std::unique_ptr<UnifiedSystemTrayBubble> bubble_;

  // Model class that stores `UnifiedSystemTray`'s UI specific variables.
  scoped_refptr<UnifiedSystemTrayModel> model_;

  const std::unique_ptr<UnifiedSliderBubbleController>
      slider_bubble_controller_;

  const std::unique_ptr<PrivacyScreenToastController>
      privacy_screen_toast_controller_;

  std::unique_ptr<AutozoomToastController> autozoom_toast_controller_;

  // Owned by the views hierarchy.
  raw_ptr<CurrentLocaleView> current_locale_view_ = nullptr;
  raw_ptr<ImeModeView> ime_mode_view_ = nullptr;
  raw_ptr<ManagedDeviceTrayItemView> managed_device_view_ = nullptr;
  raw_ptr<CameraMicTrayItemView> camera_view_ = nullptr;
  raw_ptr<CameraMicTrayItemView> mic_view_ = nullptr;
  raw_ptr<TimeTrayItemView> time_view_ = nullptr;
  raw_ptr<HotspotTrayView> hotspot_tray_view_ = nullptr;
  raw_ptr<NetworkTrayView> network_tray_view_ = nullptr;
  raw_ptr<ChannelIndicatorView> channel_indicator_view_ = nullptr;
  raw_ptr<PowerTrayView> power_tray_view_ = nullptr;

  // Contains all tray items views added to tray_container().
  std::list<raw_ptr<TrayItemView, CtnExperimental>> tray_items_;

  bool first_interaction_recorded_ = false;

  base::ObserverList<Observer> observers_;

  display::ScopedDisplayObserver display_observer_{this};

  // Records time the QS bubble was shown. Used for metrics.
  base::TimeTicks time_opened_;

  base::WeakPtrFactory<UnifiedSystemTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_H_
