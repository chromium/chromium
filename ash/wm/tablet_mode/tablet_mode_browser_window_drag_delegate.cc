// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"

#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_session_windows_hider.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The scale factor that the source window should scale if the source window is
// not the dragged window && is not in splitscreen when drag starts && the user
// has dragged the window to pass the |kIndicatorThresholdRatio| vertical
// threshold.
constexpr float kSourceWindowScale = 0.85;

// Threshold of the fling velocity to keep the dragged window as a new separate
// window after drag ends and do not try to merge it back into source window.
constexpr float kFlingToStayAsNewWindowThreshold = 2000.f;

// The class to observe the source window's bounds change animation. It's used
// to prevent the dragged window to merge back into the source window during
// dragging. Only when the source window restores to its maximized window size,
// the dragged window can be merged back into the source window.
class SourceWindowAnimationObserver : public ui::ImplicitAnimationObserver,
                                      public aura::WindowObserver {
 public:
  SourceWindowAnimationObserver(aura::Window* source_window,
                                aura::Window* dragged_window)
      : source_window_(source_window), dragged_window_(dragged_window) {
    source_window_->AddObserver(this);
    dragged_window_->AddObserver(this);
  }

  SourceWindowAnimationObserver(const SourceWindowAnimationObserver&) = delete;
  SourceWindowAnimationObserver& operator=(
      const SourceWindowAnimationObserver&) = delete;

  ~SourceWindowAnimationObserver() override { StopObserving(); }

  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {
    DCHECK(dragged_window_ && source_window_);
    dragged_window_->SetProperty(chromeos::kCanAttachToAnotherWindowKey, false);
  }

  void OnImplicitAnimationsCompleted() override {
    DCHECK(dragged_window_ && source_window_);
    // When arriving here, we know the source window bounds change animation
    // just ended. Only clear the property
    // chromeos::kCanAttachToAnotherWindowKey if the source window bounds
    // restores to its maximized window size.
    gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(source_window_)
                                     .work_area();
    ::wm::ConvertRectFromScreen(source_window_->parent(), &work_area_bounds);
    if (source_window_->bounds() == work_area_bounds)
      StopObserving();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window == source_window_ || window == dragged_window_);
    StopObserving();
  }

 private:
  void StopObserving() {
    StopObservingImplicitAnimations();
    if (source_window_) {
      source_window_->RemoveObserver(this);
      source_window_ = nullptr;
    }

    if (dragged_window_) {
      dragged_window_->RemoveObserver(this);
      dragged_window_->ClearProperty(chromeos::kCanAttachToAnotherWindowKey);
      dragged_window_ = nullptr;
    }
  }

  aura::Window* source_window_;
  aura::Window* dragged_window_;
};

}  // namespace

TabletModeBrowserWindowDragDelegate::TabletModeBrowserWindowDragDelegate() =
    default;

TabletModeBrowserWindowDragDelegate::~TabletModeBrowserWindowDragDelegate() =
    default;

void TabletModeBrowserWindowDragDelegate::PrepareWindowDrag(
    const gfx::PointF& location_in_screen) {
  DCHECK(dragged_window_);

  WindowState* window_state = WindowState::Get(dragged_window_);
  window_state->OnDragStarted(window_state->drag_details()->window_component);
}

void TabletModeBrowserWindowDragDelegate::UpdateWindowDrag(
    const gfx::PointF& location_in_screen) {
  DCHECK(dragged_window_);

  // Update the source window if necessary.
  UpdateSourceWindow(location_in_screen);
}

void TabletModeBrowserWindowDragDelegate::EndingWindowDrag(
    ToplevelWindowEventHandler::DragResult result,
    const gfx::PointF& location_in_screen) {
  if (result == ToplevelWindowEventHandler::DragResult::SUCCESS)
    WindowState::Get(dragged_window_)->OnCompleteDrag(location_in_screen);
  else
    WindowState::Get(dragged_window_)->OnRevertDrag(location_in_screen);
}

void TabletModeBrowserWindowDragDelegate::EndedWindowDrag(
    const gfx::PointF& location_in_screen) {
  MergeBackToSourceWindowIfApplicable(location_in_screen);
}

void TabletModeBrowserWindowDragDelegate::StartFling(
    const ui::GestureEvent* event) {
  if (event->details().velocity_y() > kFlingToStayAsNewWindowThreshold)
    can_merge_back_to_source_window_ = false;
}

bool TabletModeBrowserWindowDragDelegate::ShouldOpenOverviewWhenDragStarts() {
  DCHECK(dragged_window_);
  aura::Window* source_window =
      dragged_window_->GetProperty(kTabDraggingSourceWindowKey);
  return !source_window;
}

void TabletModeBrowserWindowDragDelegate::UpdateSourceWindow(
    const gfx::PointF& location_in_screen) {
  // Only do the scale if the source window is not the dragged window && the
  // source window is not in splitscreen && the source window is not in
  // overview.
  aura::Window* source_window =
      dragged_window_->GetProperty(kTabDraggingSourceWindowKey);
  if (!source_window || source_window == dragged_window_ ||
      split_view_controller_->IsWindowInSplitView(source_window) ||
      source_window->GetProperty(chromeos::kIsShowingInOverviewKey)) {
    return;
  }

  // Only create WindowHider if we need to scale up/down the source window.
  if (!windows_hider_)
    windows_hider_ =
        std::make_unique<TabletModeBrowserWindowDragSessionWindowsHider>(
            source_window, dragged_window_);

  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  gfx::Rect expected_bounds(work_area_bounds);
  if (location_in_screen.y() >=
      GetIndicatorsVerticalThreshold(work_area_bounds)) {
    SplitViewController::SnapPosition snap_position =
        GetSnapPosition(location_in_screen);

    if (snap_position == SplitViewController::SnapPosition::kNone) {
      // Scale down the source window if the event location passes the vertical
      // |kIndicatorThresholdRatio| threshold.
      expected_bounds.ClampToCenteredSize(
          gfx::Size(work_area_bounds.width() * kSourceWindowScale,
                    work_area_bounds.height() * kSourceWindowScale));
    } else {
      // Put the source window on the other side of the split screen.
      expected_bounds = split_view_controller_->GetSnappedWindowBoundsInScreen(
          snap_position == SplitViewController::SnapPosition::kPrimary
              ? SplitViewController::SnapPosition::kSecondary
              : SplitViewController::SnapPosition::kPrimary,
          source_window);
    }
  }
  ::wm::ConvertRectFromScreen(source_window->parent(), &expected_bounds);

  if (expected_bounds != source_window->GetTargetBounds()) {
    ui::ScopedLayerAnimationSettings settings(
        source_window->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    source_window_bounds_observer_ =
        std::make_unique<SourceWindowAnimationObserver>(source_window,
                                                        dragged_window_);
    settings.AddObserver(source_window_bounds_observer_.get());
    source_window->SetBounds(expected_bounds);
  }
}

void TabletModeBrowserWindowDragDelegate::MergeBackToSourceWindowIfApplicable(
    const gfx::PointF& location_in_screen) {
  // No need to merge back if we're not in tab dragging process.
  if (!window_util::IsDraggingTabs(dragged_window_))
    return;

  aura::Window* source_window =
      dragged_window_->GetProperty(kTabDraggingSourceWindowKey);
  // Do not merge back if there is no source window.
  if (!source_window)
    return;

  // Do not merge back if the dragged window is not capable of merging back.
  // This may happen if the drag ends because of a fling event and the fling
  // velocity has exceeded kFlingToStayAsNewWindowThreshold.
  if (!can_merge_back_to_source_window_)
    return;

  // Do not merge back if the window has dragged farther than half of the screen
  // height.
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  if (location_in_screen.y() >= work_area_bounds.CenterPoint().y())
    return;

  SplitViewController::SnapPosition desired_snap_position =
      GetSnapPosition(location_in_screen);
  // If splitscreen is not active, do not merge back if the dragged window is
  // in the drag-to-snap preview area.
  if (!split_view_controller_->InSplitViewMode() &&
      desired_snap_position != SplitViewController::SnapPosition::kNone) {
    return;
  }

  // In splitscreen, do not merge back if the drag point is on one side of the
  // split view divider and the source window is snapped on the opposite side.
  if (split_view_controller_->InSplitViewMode()) {
    const int drag_position = IsCurrentScreenOrientationLandscape()
                                  ? location_in_screen.x()
                                  : location_in_screen.y();
    const bool is_dragging_on_left =
        IsCurrentScreenOrientationPrimary()
            ? drag_position < split_view_controller_->divider_position()
            : drag_position > split_view_controller_->divider_position();
    aura::Window* window_on_opposite_side =
        is_dragging_on_left ? split_view_controller_->secondary_window()
                            : split_view_controller_->primary_window();
    if (source_window == window_on_opposite_side)
      return;
  }

  // Arriving here we know the dragged window should merge back into its source
  // window.
  source_window->SetProperty(chromeos::kIsDeferredTabDraggingTargetWindowKey,
                             true);
}

}  // namespace ash
