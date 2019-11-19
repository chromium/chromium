// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/animation_delegate_views.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

class DetailedViewController;
class FeaturePodControllerBase;
class PaginationController;
class UnifiedBrightnessSliderController;
class UnifiedVolumeSliderController;
class UnifiedSystemTrayBubble;
class UnifiedSystemTrayModel;
class UnifiedSystemTrayView;

// Controller class of UnifiedSystemTrayView. Handles events of the view.
class ASH_EXPORT UnifiedSystemTrayController
    : public views::AnimationDelegateViews,
      public UnifiedVolumeSliderController::Delegate {
 public:
  UnifiedSystemTrayController(UnifiedSystemTrayModel* model,
                              UnifiedSystemTrayBubble* bubble = nullptr,
                              views::View* owner_view = nullptr);
  ~UnifiedSystemTrayController() override;

  // Create the view. The created view is unowned.
  UnifiedSystemTrayView* CreateView();

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
  // Show enterprise managed device info. Called from the view.
  void HandleEnterpriseInfoAction();
  // Toggle expanded state of UnifiedSystemTrayView. Called from the view.
  void ToggleExpanded();
  // Called when message center visibility is changed. Called from the
  // view.
  void OnMessageCenterVisibilityUpdated();

  // Handle finger dragging and expand/collapse the view. Called from view.
  void BeginDrag(const gfx::Point& location);
  void UpdateDrag(const gfx::Point& location);
  void EndDrag(const gfx::Point& location);
  void Fling(int velocity);

  // Show user selector view. Called from the view.
  void ShowUserChooserView();
  // Show the detailed view of network. If |force| is true, it shows the
  // detailed view even if it's collapsed. Called from the view.
  void ShowNetworkDetailedView(bool force);
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
  // Show the detailed view of notifier settings. Called from the view.
  void ShowNotifierSettingsView();

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

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // UnifiedVolumeSliderController::Delegate:
  void OnAudioSettingsButtonClicked() override;

  UnifiedSystemTrayModel* model() { return model_; }

  PaginationController* pagination_controller() {
    return pagination_controller_.get();
  }

 private:
  friend class UnifiedSystemTrayControllerTest;
  friend class UnifiedMessageCenterBubbleTest;

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

  // Initialize feature pod controllers and their views.
  // If you want to add a new feature pod item, you have to add here.
  void InitFeaturePods();

  // Add the feature pod controller and its view.
  void AddFeaturePodItem(std::unique_ptr<FeaturePodControllerBase> controller);

  // Show the detailed view.
  void ShowDetailedView(std::unique_ptr<DetailedViewController> controller);

  // Update how much the view is expanded based on |animation_|.
  void UpdateExpandedAmount();

  // Return touch drag amount between 0.0 and 1.0. If expanding, it increases
  // towards 1.0. If collapsing, it decreases towards 0.0. If the view is
  // dragged to the same direction as the current state, it does not change the
  // value. For example, if the view is expanded and it's dragged to the top, it
  // keeps returning 1.0.
  double GetDragExpandedAmount(const gfx::Point& location) const;

  // Return true if UnifiedSystemTray is expanded.
  bool IsExpanded() const;

  // Starts animation to expand or collapse the bubble.
  void StartAnimation(bool expand);

  // Model that stores UI specific variables. Unowned.
  UnifiedSystemTrayModel* const model_;

  // Unowned. Owned by Views hierarchy.
  UnifiedSystemTrayView* unified_view_ = nullptr;

  // Unowned.
  UnifiedSystemTrayBubble* bubble_ = nullptr;

  // The controller of the current detailed view. If the main view is shown,
  // it's null. Owned.
  std::unique_ptr<DetailedViewController> detailed_view_controller_;

  // Controllers of feature pod buttons. Owned by this.
  std::vector<std::unique_ptr<FeaturePodControllerBase>>
      feature_pod_controllers_;

  std::unique_ptr<PaginationController> pagination_controller_;

  // Controller of volume slider. Owned.
  std::unique_ptr<UnifiedVolumeSliderController> volume_slider_controller_;

  // Controller of brightness slider. Owned.
  std::unique_ptr<UnifiedBrightnessSliderController>
      brightness_slider_controller_;

  // If the previous state is expanded or not. Only valid during dragging (from
  // BeginDrag to EndDrag).
  bool was_expanded_ = true;

  // The last |location| passed to BeginDrag(). Only valid during dragging.
  gfx::Point drag_init_point_;

  // Animation between expanded and collapsed states.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
