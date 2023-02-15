// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "ash/constants/app_types.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/scoped_window_tucker.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_default_layout_manager.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/constants.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

using MagnetismCorner = ash::FloatController::MagnetismCorner;

namespace ash {

namespace {

constexpr char kFloatWindowCountsPerSessionHistogramName[] =
    "Ash.Float.FloatWindowCountsPerSession";
constexpr char kFloatWindowDurationHistogramName[] =
    "Ash.Float.FloatWindowDuration";
constexpr char kFloatWindowMoveToAnotherDeskCountsHistogramName[] =
    "Ash.Float.FloatWindowMoveToAnotherDeskCounts";

// Disables the window's position auto management and returns its original
// value.
bool DisableAndGetOriginalPositionAutoManaged(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  const bool original_position_auto_managed =
      window_state->GetWindowPositionManaged();
  // Floated window position should not be auto-managed.
  if (original_position_auto_managed)
    window_state->SetWindowPositionManaged(false);
  return original_position_auto_managed;
}

// Updates `window`'s bounds while in tablet mode, using the given
// `animation_type`. Called after a drag is completed, switching between
// clamshell to tablet, and to tuck and untuck the window.
void UpdateWindowBoundsForTablet(
    aura::Window* window,
    WindowState::BoundsChangeAnimationType animation_type) {
  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);
  // TODO(b/264962634): Remove this workaround.
  // Currently `TabletModeWindowState::UpdateWindowPosition` uses
  // `Window::SetBoundsDirect` which directly changes the bounds without waiting
  // for the ack from clients (e.g. ARC++). So we need to ensure to emit
  // `SetBoundsWMEvent` instead. Otherwise, the window bounds are updated only
  // in Chrome-side whereas ARC++ doesn’t know the changes. (See comments in
  // `TabletModeWindowState::UpdateWindowPosition`.)
  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(AppType::ARC_APP)) {
    const SetBoundsWMEvent event(
        TabletModeWindowState::GetBoundsInTabletMode(window_state),
        /*animate=*/animation_type !=
            WindowState::BoundsChangeAnimationType::kNone);
    window_state->OnWMEvent(&event);
    return;
  }
  TabletModeWindowState::UpdateWindowPosition(window_state, animation_type);
}

// Hides the given floated window.
void HideFloatedWindow(aura::Window* floated_window) {
  // Disable the window animation here, because during desk deactivation we
  // are taking a screenshot of the desk (used for desk switch animations.)
  // while the `Hide()` animation is still in progress, and this will
  // introduce a glitch.
  DCHECK(floated_window);
  ScopedAnimationDisabler disabler(floated_window);
  floated_window->Hide();
}

// Shows the given floated window.
void ShowFloatedWindow(aura::Window* floated_window) {
  DCHECK(floated_window);
  if (floated_window->IsVisible()) {
    return;
  }

  ScopedAnimationDisabler disabler(floated_window);
  floated_window->Show();
}

class FloatLayoutManager : public WmDefaultLayoutManager {
 public:
  FloatLayoutManager() = default;
  FloatLayoutManager(const FloatLayoutManager&) = delete;
  FloatLayoutManager& operator=(const FloatLayoutManager&) = delete;
  ~FloatLayoutManager() override = default;

  // WmDefaultLayoutManager:
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    // This should result in sending a bounds change WMEvent to properly support
    // client-controlled windows (e.g. ARC++).
    WindowState* window_state = WindowState::Get(child);
    SetBoundsWMEvent event(requested_bounds);
    window_state->OnWMEvent(&event);
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// FloatedWindowInfo:

// Represents and stores information used for window's floated state.
class FloatController::FloatedWindowInfo : public aura::WindowObserver {
 public:
  FloatedWindowInfo(aura::Window* floated_window, const Desk* desk)
      : floated_window_(floated_window),
        was_position_auto_managed_(
            DisableAndGetOriginalPositionAutoManaged(floated_window)),
        desk_(desk) {
    DCHECK(floated_window_);
    floated_window_observation_.Observe(floated_window);

    if (desk->is_active())
      float_start_time_ = base::TimeTicks::Now();
  }

  FloatedWindowInfo(const FloatedWindowInfo&) = delete;
  FloatedWindowInfo& operator=(const FloatedWindowInfo&) = delete;
  ~FloatedWindowInfo() override {
    // Reset the window position auto-managed status if it was auto managed.
    if (was_position_auto_managed_)
      WindowState::Get(floated_window_)->SetWindowPositionManaged(true);
    MaybeRecordFloatWindowDuration();
  }

  const Desk* desk() const { return desk_; }
  void set_desk(const Desk* desk) { desk_ = desk; }

  bool is_tucked_for_tablet() const { return is_tucked_for_tablet_; }

  MagnetismCorner magnetism_corner() const { return magnetism_corner_; }
  void set_magnetism_corner(MagnetismCorner magnetism_corner) {
    magnetism_corner_ = magnetism_corner;
  }

  void MaybeRecordFloatWindowDuration() {
    if (!float_start_time_.is_null()) {
      base::UmaHistogramCustomCounts(
          kFloatWindowDurationHistogramName,
          (base::TimeTicks::Now() - float_start_time_).InMinutes(), 1,
          base::Days(7).InMinutes(), 50);
      float_start_time_ = base::TimeTicks();
    }
  }

  void MaybeTuckWindow(bool left) {
    // The order here matters: `is_tucked_for_tablet_` must be set to true
    // while in the constructor and also before `AnimateUntuck()` gets the
    // tucked window bounds.
    is_tucked_for_tablet_ = true;
    scoped_window_tucker_ =
        std::make_unique<ScopedWindowTucker>(floated_window_, left);
    scoped_window_tucker_->AnimateTuck();
  }

  void OnUntuckAnimationEnded() { scoped_window_tucker_.reset(); }

  void MaybeUntuckWindow() {
    // The order here matters: `is_tucked_for_tablet_` must be set to false
    // before `AnimateUntuck()` gets the untucked window bounds.
    is_tucked_for_tablet_ = false;
    if (scoped_window_tucker_) {
      scoped_window_tucker_->AnimateUntuck(
          base::BindOnce(&FloatedWindowInfo::OnUntuckAnimationEnded,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  views::Widget* GetTuckHandleWidget() {
    DCHECK(scoped_window_tucker_);
    return scoped_window_tucker_->tuck_handle_widget();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(floated_window_, window);
    DCHECK(floated_window_observation_.IsObservingSource(floated_window_));
    // Note that `this` is deleted below in `OnFloatedWindowDestroying()` and
    // should not be accessed after this.
    Shell::Get()->float_controller()->OnFloatedWindowDestroying(window);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window != floated_window_)
      return;

    // When a floated window switches desks, it is hidden or shown. We track the
    // amount of time a floated window is visible on the active desk to avoid
    // recording the cases if a floated window is floated indefinitely on an
    // inactive desk. Check if the desk is active as well, as some UI such as
    // the saved desks library view may temporarily hide the floated window on
    // the active desk.
    if (visible && desk_->is_active()) {
      if (float_start_time_.is_null())
        float_start_time_ = base::TimeTicks::Now();
      return;
    }

    if (!visible && !desk_->is_active())
      MaybeRecordFloatWindowDuration();
  }

 private:
  // The `floated_window` this object is hosting information for.
  aura::Window* floated_window_;

  // When a window is floated, the window position should not be auto-managed.
  // Use this value to reset the auto-managed state when unfloating a window.
  const bool was_position_auto_managed_;

  // Scoped object that handles the special tucked window state, which is not
  // a normal window state. Null when `floated_window_` is currently not tucked.
  std::unique_ptr<ScopedWindowTucker> scoped_window_tucker_;

  // Used to get the tucked window bounds (as opposed to normal floated). False
  // during `scoped_window_tucker_` construction.
  bool is_tucked_for_tablet_ = false;

  // The desk where floated window belongs to.
  // When a window is getting floated, it moves from desk container to float
  // container, this Desk pointer is used to determine floating window's desk
  // ownership, since floated window should only be shown on the desk it belongs
  // to.
  const Desk* desk_;

  // The start time when the floated window is on the active desk. Used for
  // logging the amount of time a window is floated. Logged when the desk
  // changes to inactive (when combining desks we can change desks, but remain
  // on the active desk), or when the window is unfloated.
  base::TimeTicks float_start_time_;

  // The corner the `floated_window_` should be magnetized to.
  // By default it magnetizes to the bottom right when first floated.
  MagnetismCorner magnetism_corner_ = MagnetismCorner::kBottomRight;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      floated_window_observation_{this};

  base::WeakPtrFactory<FloatedWindowInfo> weak_ptr_factory_{this};
};

// -----------------------------------------------------------------------------
// FloatController:

FloatController::FloatController() {
  shell_observation_.Observe(Shell::Get());
  for (aura::Window* root : Shell::GetAllRootWindows())
    OnRootWindowAdded(root);
}

FloatController::~FloatController() {
  // Record how many windows are floated per session.
  base::UmaHistogramCounts100(kFloatWindowCountsPerSessionHistogramName,
                              floated_window_counter_);
  // Record how many windows are moved to another desk per session.
  base::UmaHistogramCounts100(kFloatWindowMoveToAnotherDeskCountsHistogramName,
                              floated_window_move_to_another_desk_counter_);
}

// static
gfx::Rect FloatController::GetPreferredFloatWindowClamshellBounds(
    aura::Window* window) {
  DCHECK(chromeos::wm::CanFloatWindow(window));

  // In the case of window restore, as we re-float previously floated window, we
  // will use `window->bounds()`to restore floated window's previous
  // location.
  if (window->GetProperty(app_restore::kLaunchedFromAppRestoreKey))
    return window->bounds();

  gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                            ->user_work_area_bounds();
  wm::ConvertRectFromScreen(window->GetRootWindow(), &work_area);

  // Default float size is 1/3 width and 70% height of `work_area`.
  // Float bounds also should not be smaller than min bounds, use min
  // width/height if it exceeds the limit.
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  gfx::Rect preferred_bounds =
      gfx::Rect(std::max(static_cast<int>(work_area.width() * 0.33),
                         minimum_size.width()),
                std::max(static_cast<int>(work_area.height() * 0.7),
                         minimum_size.height()));

  // If user has already adjusted the window to be a size smaller than the
  // calculated preferred size, use user size instead.
  if (window->bounds().height() <= preferred_bounds.height() &&
      window->bounds().width() <= preferred_bounds.width()) {
    preferred_bounds = window->bounds();
  }

  const int padding_dp = chromeos::wm::kFloatedWindowPaddingDp;
  const int preferred_width =
      std::min(preferred_bounds.width(), work_area.width() - 2 * padding_dp);
  const int preferred_height =
      std::min(preferred_bounds.height(), work_area.height() - 2 * padding_dp);

  return gfx::Rect(work_area.right() - preferred_width - padding_dp,
                   work_area.bottom() - preferred_height - padding_dp,
                   preferred_width, preferred_height);
}

// static
gfx::Rect FloatController::GetPreferredFloatWindowTabletBounds(
    aura::Window* window) {
  gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                            ->user_work_area_bounds();
  wm::ConvertRectFromScreen(window->GetRootWindow(), &work_area);

  const bool landscape = chromeos::wm::IsLandscapeOrientationForWindow(window);
  const gfx::Size preferred_size =
      chromeos::wm::GetPreferredFloatedWindowTabletSize(work_area, landscape);
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();

  const int width = std::max(preferred_size.width(), minimum_size.width());

  // Preferred height is always greater than minimum height since this function
  // won't be called otherwise.
  DCHECK_GT(preferred_size.height(), minimum_size.height());
  const int height = preferred_size.height();

  // Get `floated_window_info` from the float controller. For non ARC apps, it
  // is expected we call this function on already floated windows.
  auto* floated_window_info =
      Shell::Get()->float_controller()->MaybeGetFloatedWindowInfo(window);
#if DCHECK_IS_ON()
  if (window->GetProperty(aura::client::kAppType) !=
      static_cast<int>(AppType::ARC_APP)) {
    DCHECK(floated_window_info);
  }
#endif

  // Update the origin of the floated window based on whichever corner it is
  // magnetized to.
  gfx::Point origin;

  const MagnetismCorner magnetism_corner =
      floated_window_info ? floated_window_info->magnetism_corner()
                          : MagnetismCorner::kBottomRight;
  const int padding_dp = chromeos::wm::kFloatedWindowPaddingDp;
  switch (magnetism_corner) {
    case MagnetismCorner::kTopLeft:
      origin =
          gfx::Point(work_area.x() + padding_dp, work_area.y() + padding_dp);
      break;
    case MagnetismCorner::kTopRight:
      origin = gfx::Point(work_area.right() - width - padding_dp,
                          work_area.y() + padding_dp);
      break;
    case MagnetismCorner::kBottomLeft:
      origin = gfx::Point(work_area.x() + padding_dp,
                          work_area.bottom() - height - padding_dp);
      break;
    case MagnetismCorner::kBottomRight:
      origin = gfx::Point(work_area.right() - width - padding_dp,
                          work_area.bottom() - height - padding_dp);
      break;
  }

  // If the window is tucked, shift it so the window is offscreen.
  if (floated_window_info && floated_window_info->is_tucked_for_tablet()) {
    int x_offset;
    switch (magnetism_corner) {
      case MagnetismCorner::kTopLeft:
      case MagnetismCorner::kBottomLeft:
        x_offset = -width - padding_dp;
        break;
      case MagnetismCorner::kTopRight:
      case MagnetismCorner::kBottomRight:
        x_offset = width + padding_dp;
        break;
    }
    origin.Offset(x_offset, 0);
  }

  return gfx::Rect(origin, gfx::Size(width, height));
}

void FloatController::MaybeUntuckFloatedWindowForTablet(
    aura::Window* floated_window) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  floated_window_info->MaybeUntuckWindow();
}

bool FloatController::IsFloatedWindowTuckedForTablet(
    const aura::Window* floated_window) const {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  // This can be called during state transition, where window is getting into
  // float state but float info is not created, return false as default tuck
  // status is false.
  if (!floated_window_info) {
    return false;
  }
  return floated_window_info->is_tucked_for_tablet();
}

bool FloatController::IsFloatedWindowAlignedWithShelf(
    aura::Window* floated_window) const {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  if (floated_window_info->is_tucked_for_tablet()) {
    return false;
  }

  MagnetismCorner magnetism_corner = floated_window_info->magnetism_corner();
  return magnetism_corner == MagnetismCorner::kBottomLeft ||
         magnetism_corner == MagnetismCorner::kBottomRight;
}

views::Widget* FloatController::GetTuckHandleWidget(
    const aura::Window* floated_window) const {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  return floated_window_info->GetTuckHandleWidget();
}

void FloatController::OnDragCompletedForTablet(aura::Window* floated_window) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);

  // Use the display bounds since the user may drag on to the shelf or spoken
  // feedback bar.
  const gfx::Point display_bounds_center =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(floated_window->GetRootWindow())
          .bounds()
          .CenterPoint();
  const int display_bounds_center_x = display_bounds_center.x();
  const int display_bounds_center_y = display_bounds_center.y();

  // Check which corner to magnetize to based on which quadrant of the display
  // the centerpoint of the window was on touch released. Not that the
  // centerpoint may be offscreen.
  const gfx::Point float_window_center =
      floated_window->GetBoundsInScreen().CenterPoint();
  const int float_window_center_x = float_window_center.x();
  const int float_window_center_y = float_window_center.y();
  MagnetismCorner magnetism_corner;
  if (float_window_center_x < display_bounds_center_x &&
      float_window_center_y < display_bounds_center_y) {
    magnetism_corner = MagnetismCorner::kTopLeft;
  } else if (float_window_center_x >= display_bounds_center_x &&
             float_window_center_y < display_bounds_center_y) {
    magnetism_corner = MagnetismCorner::kTopRight;
  } else if (float_window_center_x < display_bounds_center_x &&
             float_window_center_y >= display_bounds_center_y) {
    magnetism_corner = MagnetismCorner::kBottomLeft;
  } else {
    DCHECK_GE(float_window_center_x, display_bounds_center_x);
    DCHECK_GE(float_window_center_y, display_bounds_center_y);
    magnetism_corner = MagnetismCorner::kBottomRight;
  }

  floated_window_info->set_magnetism_corner(magnetism_corner);
  UpdateWindowBoundsForTablet(floated_window,
                              WindowState::BoundsChangeAnimationType::kAnimate);
}

void FloatController::OnFlingOrSwipeForTablet(aura::Window* floated_window,
                                              float velocity_x,
                                              float velocity_y) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  // Move the window in the direction of the vertical velocity.
  MagnetismCorner magnetism_corner = floated_window_info->magnetism_corner();
  bool start_left = magnetism_corner == MagnetismCorner::kTopLeft ||
                    magnetism_corner == MagnetismCorner::kBottomLeft;
  if (velocity_y < 0.f) {
    floated_window_info->set_magnetism_corner(
        start_left ? MagnetismCorner::kTopLeft : MagnetismCorner::kTopRight);
  } else if (velocity_y > 0.f) {
    floated_window_info->set_magnetism_corner(
        start_left ? MagnetismCorner::kBottomLeft
                   : MagnetismCorner::kBottomRight);
  }

  // Move the window in the direction of the horizontal velocity. Note that the
  // updated `magnetism_corner()` must be used to get the direction of both
  // velocities.
  magnetism_corner = floated_window_info->magnetism_corner();
  bool start_top = magnetism_corner == MagnetismCorner::kTopLeft ||
                   magnetism_corner == MagnetismCorner::kTopRight;
  if (velocity_x < 0.f) {
    floated_window_info->set_magnetism_corner(
        start_top ? MagnetismCorner::kTopLeft : MagnetismCorner::kBottomLeft);
  } else if (velocity_x > 0.f) {
    floated_window_info->set_magnetism_corner(
        start_top ? MagnetismCorner::kTopRight : MagnetismCorner::kBottomRight);
  }

  // If the horizontal velocity was in the direction of `start` tuck the
  // window, otherwise magnetize it.
  if ((start_left && velocity_x < 0.f) || (!start_left && velocity_x > 0.f)) {
    floated_window_info->MaybeTuckWindow(start_left);
    return;
  }
  UpdateWindowBoundsForTablet(floated_window,
                              WindowState::BoundsChangeAnimationType::kAnimate);
}

const Desk* FloatController::FindDeskOfFloatedWindow(
    const aura::Window* window) const {
  if (auto* info = MaybeGetFloatedWindowInfo(window))
    return info->desk();
  return nullptr;
}

aura::Window* FloatController::FindFloatedWindowOfDesk(const Desk* desk) const {
  DCHECK(desk);
  for (const auto& [window, info] : floated_window_info_map_) {
    if (info->desk() == desk)
      return window;
  }
  return nullptr;
}

void FloatController::OnMovingAllWindowsOutToDesk(Desk* original_desk,
                                                  Desk* target_desk) {
  auto* original_desk_floated_window = FindFloatedWindowOfDesk(original_desk);
  if (!original_desk_floated_window)
    return;
  // Records floated window being moved to another desk.
  ++floated_window_move_to_another_desk_counter_;
  auto* target_desk_floated_window = FindFloatedWindowOfDesk(target_desk);

  // Float window might have been hidden on purpose and won't show
  // automatically.
  ShowFloatedWindow(original_desk_floated_window);
  // During desk removal/combine, if `target_desk` has a floated window, we
  // will unfloat the floated window in `original_desk` and re-parent it back
  // to its desk container.
  if (target_desk_floated_window) {
    // Unfloat the floated window at `original_desk` desk.
    ResetFloatedWindow(original_desk_floated_window);
  } else {
    floated_window_info_map_[original_desk_floated_window]->set_desk(
        target_desk);
    // Note that other windows that belong to the "same container"
    //  are being re-sorted at the end of
    // `Desk::MoveWindowsToDesk`. This ensures windows associated with removed
    // desk appear as least recent in MRU order, since they get appended at
    // the end of overview. we are calling it here so the floated window
    // that's being moved to the target desk is also being sorted for the same
    // reason.
    Shell::Get()->mru_window_tracker()->OnWindowMovedOutFromRemovingDesk(
        original_desk_floated_window);
  }
}

void FloatController::OnMovingFloatedWindowToDesk(aura::Window* floated_window,
                                                  Desk* active_desk,
                                                  Desk* target_desk,
                                                  aura::Window* target_root) {
  auto* target_desk_floated_window = FindFloatedWindowOfDesk(target_desk);
  aura::Window* root = floated_window->GetRootWindow();
  if (target_desk_floated_window) {
    // Unfloat the floated window at `target_desk`.
    ResetFloatedWindow(target_desk_floated_window);
  }
  auto* float_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(float_info);
  DCHECK_EQ(float_info->desk(), active_desk);
  float_info->set_desk(target_desk);
  // Records floated window being moved to another desk.
  ++floated_window_move_to_another_desk_counter_;
  if (root != target_root) {
    // If `floated_window_` is dragged to a desk on a different display, we
    // also need to move it to the target display.
    window_util::MoveWindowToDisplay(floated_window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(target_root)
                                         .id());
  }

  // Update `floated_window` visibility based on target desk's activation
  // status.
  if (target_desk->is_active()) {
    ShowFloatedWindow(floated_window);
  } else {
    HideFloatedWindow(floated_window);
  }
  active_desk->NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

void FloatController::OnTabletModeStarted() {
  DCHECK(!floated_window_info_map_.empty());
  // If a window can still remain floated, update its bounds, otherwise unfloat
  // it. Note that the bounds update has to happen after tablet mode has started
  // as opposed to while it is still starting, since some windows change their
  // minimum size, which tablet float bounds depend on.
  std::vector<aura::Window*> windows_need_reset;
  for (auto& [window, info] : floated_window_info_map_) {
    if (chromeos::wm::CanFloatWindow(window)) {
      UpdateWindowBoundsForTablet(
          window, WindowState::BoundsChangeAnimationType::kCrossFade);
    } else {
      windows_need_reset.push_back(window);
    }
  }
  for (auto* window : windows_need_reset)
    ResetFloatedWindow(window);
}

void FloatController::OnTabletModeEnding() {
  for (auto& [window, info] : floated_window_info_map_)
    info->MaybeUntuckWindow();
}

void FloatController::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void FloatController::OnDeskActivationChanged(const Desk* activated,
                                              const Desk* deactivated) {
  // Since floated windows are not children of desk containers, switching desks
  // (which changes the visibility of desks' containers) won't automatically
  // update the floated windows' visibility. Therefore, here we hide the floated
  // window belonging to the deactivated desk, and show the one belonging to the
  // activated desk.
  if (auto* deactivated_desk_floated_window =
          FindFloatedWindowOfDesk(deactivated)) {
    HideFloatedWindow(deactivated_desk_floated_window);
  }
  if (auto* activated_desk_floated_window =
          FindFloatedWindowOfDesk(activated)) {
    ShowFloatedWindow(activated_desk_floated_window);
  }
}

void FloatController::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  // TODO(sammiequon): Make this work for clamshell mode too.
  // The work area can change while entering or exiting tablet mode. The float
  // window changes related with those changes are handled in
  // `OnTabletModeStarting`, `OnTabletModeEnding` or attaching/detaching window
  // states.
  display::TabletState tablet_state = chromeos::TabletState::Get()->state();
  if (tablet_state == display::TabletState::kEnteringTabletMode ||
      tablet_state == display::TabletState::kEnteringTabletMode) {
    return;
  }

  if ((display::DisplayObserver::DISPLAY_METRIC_WORK_AREA & metrics) == 0)
    return;

  DCHECK(!floated_window_info_map_.empty());
  std::vector<aura::Window*> windows_need_reset;
  for (auto& [window, info] : floated_window_info_map_) {
    if (!chromeos::wm::CanFloatWindow(window)) {
      windows_need_reset.push_back(window);
    } else {
      // Let the state object handle the work area change. This is normally
      // handled by the `WorkspaceLayoutManager`, but the float container does
      // not have one attached.
      const WMEvent event(WM_EVENT_WORKAREA_BOUNDS_CHANGED);
      WindowState::Get(window)->OnWMEvent(&event);
    }
  }
  for (auto* window : windows_need_reset)
    ResetFloatedWindow(window);
}

void FloatController::OnRootWindowAdded(aura::Window* root_window) {
  workspace_event_handlers_[root_window] =
      std::make_unique<WorkspaceEventHandler>(
          root_window->GetChildById(kShellWindowId_FloatContainer));
  root_window->GetChildById(kShellWindowId_FloatContainer)
      ->SetLayoutManager(std::make_unique<FloatLayoutManager>());
}

void FloatController::OnRootWindowWillShutdown(aura::Window* root_window) {
  workspace_event_handlers_.erase(root_window);
}

void FloatController::OnShellDestroying() {
  workspace_event_handlers_.clear();
}

void FloatController::ToggleFloat(aura::Window* window) {
  WindowState* window_state = WindowState::Get(window);
  const WMEvent toggle_event(window_state->IsFloated() ? WM_EVENT_RESTORE
                                                       : WM_EVENT_FLOAT);
  window_state->OnWMEvent(&toggle_event);
}

void FloatController::FloatForTablet(aura::Window* window,
                                     chromeos::WindowStateType old_state_type) {
  DCHECK(Shell::Get()->tablet_mode_controller()->InTabletMode());

  FloatImpl(window);

  if (!chromeos::IsSnappedWindowStateType(old_state_type))
    return;

  // Update magnetism so that the float window is roughly in the same location
  // as it was when it was snapped.
  const bool left_or_top =
      old_state_type == chromeos::WindowStateType::kPrimarySnapped;
  const bool landscape = IsCurrentScreenOrientationLandscape();
  MagnetismCorner magnetism_corner;
  if (!left_or_top) {
    // Bottom or right snapped.
    magnetism_corner = MagnetismCorner::kBottomRight;
  } else if (landscape) {
    // Left snapped.
    magnetism_corner = MagnetismCorner::kBottomLeft;
  } else {
    DCHECK(left_or_top && !landscape);
    // Top snapped.
    magnetism_corner = MagnetismCorner::kTopRight;
  }

  auto* floated_window_info = MaybeGetFloatedWindowInfo(window);
  DCHECK(floated_window_info);
  floated_window_info->set_magnetism_corner(magnetism_corner);
}

void FloatController::FloatImpl(aura::Window* window) {
  if (floated_window_info_map_.contains(window))
    return;

  // If a floated window already exists at current desk, unfloat it before
  // floating `window`.
  auto* desk_controller = DesksController::Get();
  // Get the desk where the window belongs to before moving it to float
  // container.
  const Desk* desk = desks_util::GetDeskForContext(window);
  DCHECK(desk);

  // TODO(b/267363112): Allow a floated window to be assigned to all desks.
  // If window is visible to all desks, unset it.
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    window->SetProperty(aura::client::kWindowWorkspaceKey,
                        aura::client::kWindowWorkspaceUnassignedWorkspace);
  }

  auto* previously_floated_window = FindFloatedWindowOfDesk(desk);
  // Add floated window to `floated_window_info_map_`.
  // Note: this has to be called before `ResetFloatedWindow`. Because in the
  // call sequence of `ResetFloatedWindow` we will access
  // `floated_window_info_map_`, and hit a corner case where window
  // `IsFloated()` returns true, but `FindDeskOfFloatedWindow` returns nullptr.
  floated_window_info_map_.emplace(
      window, std::make_unique<FloatedWindowInfo>(window, desk));
  if (previously_floated_window)
    ResetFloatedWindow(previously_floated_window);

  aura::Window* floated_container =
      window->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
  DCHECK_NE(window->parent(), floated_container);
  floated_container->AddChild(window);

  if (!desk->is_active())
    HideFloatedWindow(window);

  // Update floated window counts.
  // Note that if the same window gets floated 2 times in the same session, it's
  // counted as 2 floated windows.
  ++floated_window_counter_;

  if (!tablet_mode_observation_.IsObserving())
    tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
  if (!desks_controller_observation_.IsObserving())
    desks_controller_observation_.Observe(desk_controller);
  if (!display_observer_)
    display_observer_.emplace(this);
}

void FloatController::UnfloatImpl(aura::Window* window) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(window);
  if (!floated_window_info)
    return;

  // When a window is moved in/out from active desk container to float
  // container, it gets reparented and will use
  // `pre_added_to_workspace_window_bounds_` to update it's bounds, here we
  // update `pre_added_to_workspace_window_bounds_` as window is re-added to
  // active desk container from float container.
  WindowState::Get(window)->SetPreAddedToWorkspaceWindowBounds(
      window->bounds());
  // Floated window have been hidden on purpose on the inactive desk.
  ShowFloatedWindow(window);
  // Re-parent window to the "parent" desk's desk container.
  floated_window_info->desk()
      ->GetDeskContainerForRoot(window->GetRootWindow())
      ->AddChild(window);
  floated_window_info_map_.erase(window);
  if (floated_window_info_map_.empty()) {
    desks_controller_observation_.Reset();
    tablet_mode_observation_.Reset();
    display_observer_.reset();
  }
}

void FloatController::ResetFloatedWindow(aura::Window* floated_window) {
  DCHECK(floated_window);
  DCHECK(WindowState::Get(floated_window)->IsFloated());
  ToggleFloat(floated_window);
}

FloatController::FloatedWindowInfo* FloatController::MaybeGetFloatedWindowInfo(
    const aura::Window* window) const {
  const auto iter = floated_window_info_map_.find(window);
  if (iter == floated_window_info_map_.end())
    return nullptr;
  return iter->second.get();
}

void FloatController::OnFloatedWindowDestroying(aura::Window* floated_window) {
  floated_window_info_map_.erase(floated_window);
  if (floated_window_info_map_.empty()) {
    desks_controller_observation_.Reset();
    tablet_mode_observation_.Reset();
    display_observer_.reset();
  }
}

}  // namespace ash
