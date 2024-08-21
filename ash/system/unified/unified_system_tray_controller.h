// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/media/quick_settings_media_view_controller.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/safety_checks.h"
#include "base/memory/scoped_refptr.h"
#include "components/global_media_controls/public/constants.h"

namespace ash {

class DetailedViewController;
class FeaturePodControllerBase;
class PaginationController;
class UnifiedBrightnessSliderController;
class UnifiedVolumeSliderController;
class UnifiedSystemTrayBubble;
class UnifiedSystemTrayModel;

// Controller class of `QuickSettingsView`. Handles events of the view.
class ASH_EXPORT UnifiedSystemTrayController
    : public UnifiedVolumeSliderController::Delegate {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  class Observer : public base::CheckedObserver {
   public:
    // Gets called when `ShowCalendarView`, right as animations starts.
    virtual void OnOpeningCalendarView() {}

    // Gets called when leaving from the calendar view to main view.
    virtual void OnTransitioningFromCalendarToMainView() {}
  };

  explicit UnifiedSystemTrayController(
      scoped_refptr<UnifiedSystemTrayModel> model,
      UnifiedSystemTrayBubble* bubble = nullptr,
      views::View* owner_view = nullptr);

  UnifiedSystemTrayController(const UnifiedSystemTrayController&) = delete;
  UnifiedSystemTrayController& operator=(const UnifiedSystemTrayController&) =
      delete;

  ~UnifiedSystemTrayController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Create the view in the bubble.
  std::unique_ptr<QuickSettingsView> CreateQuickSettingsView(int max_height);

  // Sign out from the current user. Called from the view.
  void HandleSignOutAction();
  // Show lock screen which asks the user password. Called from the view.
  void HandleLockAction();
  // Show WebUI settings. Called from the view.
  void HandleSettingsAction();
  // Shutdown the computer. Called from the view.
  void HandlePowerAction();
  // Switch to page represented by it's button. Called from the view.
  void HandlePageSwitchAction(int page);
  // Show date and time settings. Called from the view.
  void HandleOpenDateTimeSettingsAction();
  // Show power settings. Called from the view.
  void HandleOpenPowerSettingsAction();
  // Show enterprise managed device info. Called from the view.
  void HandleEnterpriseInfoAction();

  // Show user selector view. Called from the view.
  void ShowUserChooserView();
  // Show the detailed view of Quick Share, formerly Nearby Share. Called from
  // the view.
  void ShowNearbyShareDetailedView();
  // Show the detailed view of network. Called from the view.
  void ShowNetworkDetailedView();
  // Show the detailed view of hotspot. Called from the view.
  void ShowHotspotDetailedView();
  // Show the detailed view of bluetooth.
  void ShowBluetoothDetailedView();
  // Show the detailed view of cast. Called from the view.
  void ShowCastDetailedView();
  // Show the detailed view of accessibility. Called from the view.
  void ShowAccessibilityDetailedView();
  // Show the detailed view of focus mode. Called from the view.
  void ShowFocusModeDetailedView();
  // Show the detailed view of VPN. Called from the view.
  void ShowVPNDetailedView();
  // Show the detailed view of IME. Called from the view.
  void ShowIMEDetailedView();
  // Show the detailed view of locale. Called from the view.
  void ShowLocaleDetailedView();
  // Show the detailed view of audio. Called from the view.
  void ShowAudioDetailedView();
  // Show the detailed view of display. Called from the view.
  void ShowDisplayDetailedView();
  // Show the detailed view of media controls. Called from the view.
  void ShowMediaControlsDetailedView(
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      const std::string& show_devices_for_item_id = "");
  // Show the detailed view of Calendar. Called from the view.
  void ShowCalendarView(calendar_metrics::CalendarViewShowSource show_source,
                        calendar_metrics::CalendarEventSource event_source);

  // If you want to add a new detailed view, add here.

  // Show the main view back from a detailed view. If |restore_focus| is true,
  // it restores previous keyboard focus in the main view. Called from a
  // detailed view controller.
  void TransitionToMainView(bool restore_focus);

  // Close the bubble. Called from a detailed view controller.
  void CloseBubble();

  // Return whether a detailed view is currently being shown.
  bool IsDetailedViewShown() const;

  // UnifiedVolumeSliderController::Delegate:
  void OnAudioSettingsButtonClicked() override;

  // Sets whether the quick settings view should show the media view.
  void SetShowMediaView(bool show_media_view);

  // Update the bubble view layout.
  void UpdateBubble();

  scoped_refptr<UnifiedSystemTrayModel> model() { return model_; }

  PaginationController* pagination_controller() {
    return pagination_controller_.get();
  }

  DetailedViewController* detailed_view_controller() {
    return detailed_view_controller_.get();
  }

  QuickSettingsMediaViewController* media_view_controller() {
    CHECK(media_view_controller_);
    return media_view_controller_.get();
  }

  bool showing_accessibility_detailed_view() const {
    return showing_accessibility_detailed_view_;
  }

  bool showing_audio_detailed_view() const {
    return showing_audio_detailed_view_;
  }

  bool showing_display_detailed_view() const {
    return showing_display_detailed_view_;
  }

  bool showing_calendar_view() const { return showing_calendar_view_; }

  void SetMediaViewControllerForTesting(
      std::unique_ptr<QuickSettingsMediaViewController> test_controller) {
    CHECK(!media_view_controller_);
    media_view_controller_ = std::move(test_controller);
  }

  void ShutDownDetailedViewController();

 private:
  friend class AccessibilityFeaturePodControllerTest;
  friend class SystemTrayTestApi;
  friend class UnifiedBrightnessViewTest;
  friend class UnifiedMessageCenterBubbleTest;
  friend class UnifiedSystemTrayControllerTest;
  friend class UnifiedVolumeViewTest;

  // Type of a help page opened by the "Managed" indicator in the bubble. The
  // enum is used to back an UMA histogram and should be treated as append-only.
  enum ManagedType { MANAGED_TYPE_ENTERPRISE = 0, MANAGED_TYPE_COUNT };

  // Initialize feature pod controllers and their feature tile views.
  void InitFeatureTiles();

  // Show the detailed view.
  void ShowDetailedView(std::unique_ptr<DetailedViewController> controller);

  bool ShouldShowDeferredUpdateDialog() const;

  // Model that stores UI specific variables. Unowned.
  scoped_refptr<UnifiedSystemTrayModel> model_;

  // Unowned. Owned by Views hierarchy.
  raw_ptr<QuickSettingsView, DanglingUntriaged> quick_settings_view_ = nullptr;

  // Unowned.
  raw_ptr<UnifiedSystemTrayBubble> bubble_ = nullptr;

  // The controller of the current detailed view. If the main view is shown,
  // it's null. Owned.
  std::unique_ptr<DetailedViewController> detailed_view_controller_;

  // Controllers of feature pod buttons. Owned by this.
  std::vector<std::unique_ptr<FeaturePodControllerBase>>
      feature_pod_controllers_;

  std::unique_ptr<PaginationController> pagination_controller_;

  std::unique_ptr<QuickSettingsMediaViewController> media_view_controller_;

  // Controller of volume slider. Owned.
  std::unique_ptr<UnifiedVolumeSliderController> volume_slider_controller_;
  raw_ptr<views::View, DanglingUntriaged> unified_volume_view_ = nullptr;

  // Controller of brightness slider. Owned.
  std::unique_ptr<UnifiedBrightnessSliderController>
      brightness_slider_controller_;
  raw_ptr<views::View, DanglingUntriaged> unified_brightness_view_ = nullptr;

  bool showing_accessibility_detailed_view_ = false;

  bool showing_audio_detailed_view_ = false;

  bool showing_display_detailed_view_ = false;

  bool showing_calendar_view_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
