// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/media/unified_media_controls_controller.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_model.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/animation_delegate_views.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace ash {

class DetailedViewController;
class FeaturePodControllerBase;
class PaginationController;
class QuickSettingsMediaViewController;
class UnifiedMediaControlsController;
class UnifiedBrightnessSliderController;
class UnifiedVolumeSliderController;
class UnifiedSystemTrayBubble;
class UnifiedSystemTrayModel;
class UnifiedSystemTrayView;

// Controller class of UnifiedSystemTrayView. Handles events of the view.
class ASH_EXPORT UnifiedSystemTrayController
    : public views::AnimationDelegateViews,
      public SessionObserver,
      public UnifiedVolumeSliderController::Delegate,
      public UnifiedMediaControlsController::Delegate {
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

  // Registers pref to preserve tray expanded state between reboots.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Create the view in the bubble.
  std::unique_ptr<UnifiedSystemTrayView> CreateUnifiedQuickSettingsView();
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
  // Toggle expanded state of UnifiedSystemTrayView. Called from the view.
  void ToggleExpanded();

  // Handle finger dragging and expand/collapse the view. Called from view.
  void BeginDrag(const gfx::PointF& location);
  void UpdateDrag(const gfx::PointF& location);
  void EndDrag(const gfx::PointF& location);
  void Fling(int velocity);

  // Show user selector view. Called from the view.
  void ShowUserChooserView();
  // Show the detailed view of network. If |force| is true, it shows the
  // detailed view even if it's collapsed. Called from the view.
  void ShowNetworkDetailedView(bool force);
  // Show the detailed view of hotspot. Called from the view.
  void ShowHotspotDetailedView();
  // Show the detailed view of bluetooth. If collapsed, it doesn't show the
  // detailed view. Called from the view.
  void ShowBluetoothDetailedView();
  // Show the detailed view of cast. Called from the view.
  void ShowCastDetailedView();
  // Show the detailed view of accessibility. Called from the view.
  void ShowAccessibilityDetailedView();
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
  // Show the detailed view of notifier settings. Called from the view.
  void ShowNotifierSettingsView();
  // Show the detailed view of media controls. Called from the view.
  void ShowMediaControlsDetailedView();
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

  // Inform UnifiedSystemTrayBubble that UnifiedSystemTrayView is requesting to
  // relinquish focus.
  bool FocusOut(bool reverse);

  // Ensure the main view is collapsed. Called from the slider bubble
  // controller.
  void EnsureCollapsed();

  // Ensure the main view is expanded. Called from the slider bubble controller.
  void EnsureExpanded();

  // Collapse the tray without animating if there isn't sufficient space for the
  // notifications area.
  void ResetToCollapsedIfRequired();

  // Collapse the tray without animating.
  void CollapseWithoutAnimating();

  // Return whether a detailed view is currently being shown.
  bool IsDetailedViewShown() const;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // UnifiedVolumeSliderController::Delegate:
  void OnAudioSettingsButtonClicked() override;

  // UnifedMediaControlsController::Delegate;
  void ShowMediaControls() override;
  void OnMediaControlsViewClicked() override;

  // Sets whether the quick settings view should show the media view.
  void SetShowMediaView(bool show_media_view);

  // Return true if UnifiedSystemTray is expanded.
  bool IsExpanded() const;

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
    DCHECK(media_view_controller_);
    return media_view_controller_.get();
  }

  bool showing_audio_detailed_view() const {
    return showing_audio_detailed_view_;
  }

  bool showing_display_detailed_view() const {
    return showing_display_detailed_view_;
  }

  bool showing_calendar_view() const { return showing_calendar_view_; }

 private:
  friend class SystemTrayTestApi;
  friend class UnifiedBrightnessViewTest;
  friend class UnifiedMessageCenterBubbleTest;
  friend class UnifiedSystemTrayControllerTest;
  friend class UnifiedVolumeViewTest;

  // How the expanded state is toggled. The enum is used to back an UMA
  // histogram and should be treated as append-only.
  enum ToggleExpandedType {
    TOGGLE_EXPANDED_TYPE_BY_BUTTON = 0,
    TOGGLE_EXPANDED_TYPE_BY_GESTURE,
    TOGGLE_EXPANDED_TYPE_COUNT
  };

  // Type of a help page opened by the "Managed" indicator in the bubble. The
  // enum is used to back an UMA histogram and should be treated as append-only.
  enum ManagedType { MANAGED_TYPE_ENTERPRISE = 0, MANAGED_TYPE_COUNT };

  // Loads the `kSystemTrayExpanded` pref to the model.
  void LoadIsExpandedPref();

  // Initialize feature pod controllers and their views.
  // If you want to add a new feature pod item, you have to add here.
  void InitFeaturePods();

  // Initialize feature pod controllers and their tile views.
  // Temporarily only adds two feature tiles and other placeholder tiles.
  // TODO(b/252871301): Create each feature's tile.
  void InitFeatureTiles();

  // Add the feature pod controller and its view.
  void AddFeaturePodItem(std::unique_ptr<FeaturePodControllerBase> controller);

  // Show the detailed view.
  void ShowDetailedView(std::unique_ptr<DetailedViewController> controller);

  // Update how much the view is expanded based on |animation_|.
  void UpdateExpandedAmount();

  // Update the gesture distance by using the tray's collapsed and expanded
  // height.
  void UpdateDragThreshold();

  // Return touch drag amount between 0.0 and 1.0. If expanding, it increases
  // towards 1.0. If collapsing, it decreases towards 0.0. If the view is
  // dragged to the same direction as the current state, it does not change the
  // value. For example, if the view is expanded and it's dragged to the top, it
  // keeps returning 1.0.
  double GetDragExpandedAmount(const gfx::PointF& location) const;

  // Return true if message center needs to be collapsed due to limited
  // screen height.
  bool IsMessageCenterCollapseRequired() const;

  // Starts animation to expand or collapse the bubble.
  void StartAnimation(bool expand);

  // views::AnimationDelegateViews:
  base::TimeDelta GetAnimationDurationForReporting() const override;

  bool ShouldShowDeferredUpdateDialog() const;

  // Model that stores UI specific variables. Unowned.
  scoped_refptr<UnifiedSystemTrayModel> model_;

  // Unowned. Owned by Views hierarchy.
  raw_ptr<UnifiedSystemTrayView, ExperimentalAsh> unified_view_ = nullptr;
  raw_ptr<QuickSettingsView, ExperimentalAsh> quick_settings_view_ = nullptr;

  // Unowned.
  raw_ptr<UnifiedSystemTrayBubble, ExperimentalAsh> bubble_ = nullptr;

  // The pref service of the currently active user. Can be null in tests.
  raw_ptr<PrefService, ExperimentalAsh> active_user_prefs_ = nullptr;

  // The controller of the current detailed view. If the main view is shown,
  // it's null. Owned.
  std::unique_ptr<DetailedViewController> detailed_view_controller_;

  // Controllers of feature pod buttons. Owned by this.
  std::vector<std::unique_ptr<FeaturePodControllerBase>>
      feature_pod_controllers_;

  std::unique_ptr<PaginationController> pagination_controller_;

  std::unique_ptr<UnifiedMediaControlsController> media_controls_controller_;
  std::unique_ptr<QuickSettingsMediaViewController> media_view_controller_;

  // Controller of volume slider. Owned.
  std::unique_ptr<UnifiedVolumeSliderController> volume_slider_controller_;
  raw_ptr<views::View, ExperimentalAsh> unified_volume_view_ = nullptr;

  // Controller of brightness slider. Owned.
  std::unique_ptr<UnifiedBrightnessSliderController>
      brightness_slider_controller_;
  raw_ptr<views::View, ExperimentalAsh> unified_brightness_view_ = nullptr;

  // If the previous state is expanded or not. Only valid during dragging (from
  // BeginDrag to EndDrag).
  bool was_expanded_ = true;

  // The last |location| passed to BeginDrag(). Only valid during dragging.
  gfx::PointF drag_init_point_;

  // Threshold in pixel that fully collapses / expands the view through gesture.
  // Used to calculate the expanded amount that corresponds to gesture location
  // during drag.
  double drag_threshold_ = 0;

  // Animation between expanded and collapsed states.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Tracks the smoothness of collapse and expand animation.
  absl::optional<ui::ThroughputTracker> animation_tracker_;

  bool showing_audio_detailed_view_ = false;

  bool showing_display_detailed_view_ = false;

  bool showing_calendar_view_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
