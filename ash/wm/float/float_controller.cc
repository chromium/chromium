// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "base/check_op.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

// The ideal dimensions of a float window before factoring in its minimum size
// (if any) is the available work area multiplied by these ratios.
constexpr float kFloatWindowTabletWidthRatio = 0.3333333f;
constexpr float kFloatWindowTabletHeightRatio = 0.8f;

// TODO(sophiewen): Remove this once the untuck window widget is implemented. It
// is temporarily here to give users a way to untuck the window.
constexpr int kTuckedFloatWindowVisibleWidth = 100;

gfx::Size GetPreferredFloatWindowTabletSize(const gfx::Rect& work_area,
                                            bool landscape) {
  // We use the landscape bounds to determine the preferred width and height,
  // even in portrait mode.
  const int landscape_width =
      landscape ? work_area.width() : work_area.height();
  const int landscape_height =
      landscape ? work_area.height() : work_area.width();
  const int preferred_width =
      static_cast<int>(landscape_width * kFloatWindowTabletWidthRatio);
  const int preferred_height = landscape_height * kFloatWindowTabletHeightRatio;
  return gfx::Size(preferred_width, preferred_height);
}

// Returns whether the display nearest `window` is in landscape orientation.
bool IsLandscapeOrientationForWindow(aura::Window* window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const chromeos::OrientationType orientation = chromeos::RotationToOrientation(
      chromeos::GetDisplayNaturalOrientation(display), display.rotation());
  return chromeos::IsLandscapeOrientation(orientation);
}

// Updates `window`'s bounds while in tablet mode.
void UpdateWindowBoundsForTablet(aura::Window* window) {
  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);
  TabletModeWindowState::UpdateWindowPosition(window_state, /*animate=*/true);
}

}  // namespace

// Scoped class which makes modifications while a window is tucked. It owns a
// widget which is used to untuck the window.
// TODO(sophiewen): Fill in this class.
class FloatController::ScopedWindowTucker {
 public:
  explicit ScopedWindowTucker(aura::Window* window) : window_(window) {
    DCHECK(window_);
  }
  ScopedWindowTucker(const ScopedWindowTucker&) = delete;
  ScopedWindowTucker& operator=(const ScopedWindowTucker&) = delete;
  ~ScopedWindowTucker() = default;

 private:
  aura::Window* window_;
};

FloatController::FloatController() = default;

FloatController::~FloatController() = default;

gfx::Rect FloatController::GetPreferredFloatWindowTabletBounds(
    aura::Window* window) {
  DCHECK(CanFloatWindowInTablet(window));
  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  const bool landscape = IsLandscapeOrientationForWindow(window);
  const gfx::Size preferred_size =
      GetPreferredFloatWindowTabletSize(work_area, landscape);
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();

  const int width = std::max(preferred_size.width(), minimum_size.width());

  // Preferred height is always greater than minimum height since this function
  // won't be called otherwise.
  DCHECK_GT(preferred_size.height(), minimum_size.height());
  const int height = preferred_size.height();

  // Update the origin of the float window based on whichever corner it is
  // magnetized to.
  gfx::Point origin;
  switch (magnetism_corner_) {
    case MagnetismCorner::kTopLeft:
      origin = gfx::Point(kFloatWindowPaddingDp, kFloatWindowPaddingDp);
      break;
    case MagnetismCorner::kTopRight:
      origin = gfx::Point(work_area.right() - width - kFloatWindowPaddingDp,
                          kFloatWindowPaddingDp);
      break;
    case MagnetismCorner::kBottomLeft:
      origin = gfx::Point(kFloatWindowPaddingDp,
                          work_area.bottom() - height - kFloatWindowPaddingDp);
      break;
    case MagnetismCorner::kBottomRight:
      origin = gfx::Point(work_area.right() - width - kFloatWindowPaddingDp,
                          work_area.bottom() - height - kFloatWindowPaddingDp);
      break;
  }

  // If the window is tucked, shift it so `kTuckedFloatWindowVisibleWidth` is
  // visible on one side, depending on `corner`.
  if (scoped_window_tucker_) {
    int x_offset;
    switch (magnetism_corner_) {
      case MagnetismCorner::kTopLeft:
      case MagnetismCorner::kBottomLeft:
        x_offset =
            -width - kFloatWindowPaddingDp + kTuckedFloatWindowVisibleWidth;
        break;
      case MagnetismCorner::kTopRight:
      case MagnetismCorner::kBottomRight:
        x_offset =
            width + kFloatWindowPaddingDp - kTuckedFloatWindowVisibleWidth;
        break;
    }
    origin.Offset(x_offset, 0);
  }

  return gfx::Rect(origin, gfx::Size(width, height));
}

// static
gfx::Rect FloatController::GetPreferredFloatWindowClamshellBounds(
    aura::Window* window) {
  DCHECK(CanFloatWindowInClamshell(window));
  auto* work_area_insets = WorkAreaInsets::ForWindow(window->GetRootWindow());
  const gfx::Rect work_area = work_area_insets->user_work_area_bounds();

  gfx::Rect preferred_bounds =
      WindowState::Get(window)->HasRestoreBounds()
          ? WindowState::Get(window)->GetRestoreBoundsInParent()
          : window->bounds();

  // Float bounds should not be smaller than min bounds.
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  DCHECK_GE(preferred_bounds.height(), minimum_size.height());
  DCHECK_GE(preferred_bounds.width(), minimum_size.width());

  int preferred_width = std::min(preferred_bounds.width(),
                                 work_area.width() - 2 * kFloatWindowPaddingDp);
  int preferred_height =
      std::min(preferred_bounds.height(),
               work_area.height() - 2 * kFloatWindowPaddingDp);

  gfx::Rect float_init_bounds(
      work_area.width() - preferred_width - kFloatWindowPaddingDp,
      work_area.height() - preferred_height - kFloatWindowPaddingDp,
      preferred_width, preferred_height);

  return float_init_bounds;
}

// static
bool FloatController::CanFloatWindowInClamshell(aura::Window* window) {
  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  if (minimum_size.width() > work_area.width() - 2 * kFloatWindowPaddingDp ||
      minimum_size.height() > work_area.height() - 2 * kFloatWindowPaddingDp) {
    return false;
  }
  return true;
}

// static
bool FloatController::CanFloatWindowInTablet(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  if (!window_state || !window_state->CanResize())
    return false;

  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  const bool landscape = IsLandscapeOrientationForWindow(window);
  const int preferred_height =
      GetPreferredFloatWindowTabletSize(work_area, landscape).height();
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  if (minimum_size.height() > preferred_height)
    return false;

  const int landscape_width =
      landscape ? work_area.width() : work_area.height();
  // The maximize size for a floated window is half the landscape width minus
  // some space for the split view divider and padding.
  if (minimum_size.width() >
      ((landscape_width - kSplitviewDividerShortSideLength) / 2 -
       kFloatWindowPaddingDp * 2)) {
    return false;
  }
  return true;
}

void FloatController::MaybeTuckFloatedWindow() {
  if (scoped_window_tucker_)
    return;

  DCHECK(float_window_);
  scoped_window_tucker_ = std::make_unique<ScopedWindowTucker>(float_window_);
  UpdateWindowBoundsForTablet(float_window_);
}

void FloatController::MaybeUntuckFloatedWindow() {
  scoped_window_tucker_.reset();
}

void FloatController::OnDragCompleted(
    const gfx::PointF& last_location_in_parent) {
  DCHECK(float_window_);

  // Use the display bounds since the user may drag on to the shelf or spoken
  // feedback bar.
  const gfx::RectF display_bounds(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(float_window_->GetRootWindow())
          .bounds());

  // Check which corner to magnetize to based on which quadrent of the display
  // the mouse/touch was released. If it somehow falls outside, then magnetize
  // to the previous location.
  gfx::RectF display_bounds_left, display_bounds_right;
  display_bounds.SplitVertically(&display_bounds_left, &display_bounds_right);
  const float center_y = display_bounds.CenterPoint().y();
  if (display_bounds_left.InclusiveContains(last_location_in_parent)) {
    magnetism_corner_ = last_location_in_parent.y() < center_y
                            ? MagnetismCorner::kTopLeft
                            : MagnetismCorner::kBottomLeft;
  } else if (display_bounds_right.InclusiveContains(last_location_in_parent)) {
    magnetism_corner_ = last_location_in_parent.y() < center_y
                            ? MagnetismCorner::kTopRight
                            : MagnetismCorner::kBottomRight;
  }

  UpdateWindowBoundsForTablet(float_window_);
}

void FloatController::OnFlingOrSwipe(bool left, bool up) {
  DCHECK(float_window_);
  if (left && up) {
    magnetism_corner_ = MagnetismCorner::kTopLeft;
  } else if (left && !up) {
    magnetism_corner_ = MagnetismCorner::kBottomLeft;
  } else if (!left && up) {
    magnetism_corner_ = MagnetismCorner::kTopRight;
  } else {
    DCHECK(!left && !up);
    magnetism_corner_ = MagnetismCorner::kBottomRight;
  }

  MaybeTuckFloatedWindow();
}

void FloatController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(float_window_, window);
  float_window_observation_.Reset();
  float_window_ = nullptr;
  scoped_window_tucker_.reset();
  tablet_mode_observation_.Reset();
  display_observer_.reset();
}

void FloatController::OnTabletModeStarting() {
  DCHECK(float_window_);
  aura::Window* floated_window = float_window_;

  if (!CanFloatWindowInTablet(floated_window))
    ResetFloatedWindow();

  MaybeUpdateWindowUIAndBoundsForTablet(floated_window);
}

void FloatController::OnTabletModeEnding() {
  DCHECK(float_window_);
  scoped_window_tucker_.reset();
  MaybeUpdateWindowUIAndBoundsForTablet(float_window_);
}

void FloatController::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void FloatController::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  DCHECK(float_window_);
  if ((display::DisplayObserver::DISPLAY_METRIC_WORK_AREA & metrics) == 0)
    return;

  if (!CanFloatWindowInTablet(float_window_))
    ResetFloatedWindow();
  else
    MaybeUpdateWindowUIAndBoundsForTablet(float_window_);
}

void FloatController::ToggleFloat(aura::Window* window) {
  WindowState* window_state = WindowState::Get(window);
  const WMEvent toggle_event(window_state->IsFloated() ? WM_EVENT_RESTORE
                                                       : WM_EVENT_FLOAT);
  window_state->OnWMEvent(&toggle_event);
}

void FloatController::Float(aura::Window* window) {
  if (window == float_window_)
    return;

  // Only one floating window is allowed, reset previously floated window.
  ResetFloatedWindow();
  DCHECK(!float_window_);
  float_window_ = window;
  // Save the window position auto-management status.
  position_auto_managed_ = WindowState::Get(window)->GetWindowPositionManaged();
  // Floated window position should not be auto-managed.
  if (position_auto_managed_)
    WindowState::Get(window)->SetWindowPositionManaged(false);
  float_window_observation_.Observe(float_window_);
  aura::Window* float_container =
      window->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
  if (window->parent() != float_container)
    float_container->AddChild(window);

  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
  display_observer_.emplace(this);
  MaybeUpdateWindowUIAndBoundsForTablet(window);
}

void FloatController::Unfloat(aura::Window* window) {
  if (window != float_window_)
    return;
  // When a window is moved in/out from active desk container to float
  // container, it gets reparented and will use
  // `pre_added_to_workspace_window_bounds_` to update it's bounds, here we
  // update `pre_added_to_workspace_window_bounds_` as window is re-added to
  // active desk container from float container.
  WindowState::Get(window)->SetPreAddedToWorkspaceWindowBounds(
      window->bounds());
  // Re-parent window to active desk container.
  desks_util::GetActiveDeskContainerForRoot(float_window_->GetRootWindow())
      ->AddChild(float_window_);
  // Reset the window position auto-managed status.
  WindowState::Get(window)->SetWindowPositionManaged(position_auto_managed_);
  float_window_observation_.Reset();
  float_window_ = nullptr;

  tablet_mode_observation_.Reset();
  display_observer_.reset();
  scoped_window_tucker_.reset();
  MaybeUpdateWindowUIAndBoundsForTablet(window);
}

void FloatController::ResetFloatedWindow() {
  if (float_window_) {
    DCHECK(WindowState::Get(float_window_)->IsFloated());
    ToggleFloat(float_window_);
    float_window_ = nullptr;
  }
}

void FloatController::MaybeUpdateWindowUIAndBoundsForTablet(
    aura::Window* window) {
  DCHECK(window);

  // Update bounds and UI when entering or exiting tablet mode, or while in
  // tablet mode.
  if (chromeos::TabletState::Get()->state() ==
      display::TabletState::kInClamshellMode) {
    return;
  }

  // TODO(sophiewen): Update rounded corners and shadow.

  UpdateWindowBoundsForTablet(window);
}

}  // namespace ash
