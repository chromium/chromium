// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "base/check_op.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/constants.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr float kTuckHandleCornerRadius = 12;
constexpr int kTuckHandleIconSize = 16;
constexpr int kTuckHandleWidth = 24;
constexpr int kTuckHandleHeight = 100;

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

// Updates `window`'s bounds while in tablet mode. Note that this uses a bounds
// animation which can be expensive. Called after a drag is completed or
// switching from clamshell to tablet or vice versa.
void UpdateWindowBoundsForTablet(aura::Window* window) {
  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);
  TabletModeWindowState::UpdateWindowPosition(
      window_state, WindowState::BoundsChangeAnimationType::kAnimate);
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
  ScopedAnimationDisabler disabler(floated_window);
  floated_window->Show();
}

}  // namespace

// -----------------------------------------------------------------------------
// FloatController::TuckHandle:

// Represents a tuck handle that untucks floated windows from offscreen.
class FloatController::TuckHandle : public views::ImageButton {
 public:
  TuckHandle(base::RepeatingClosure callback, const gfx::VectorIcon* icon)
      : views::ImageButton(callback), icon_(icon) {
    SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    SetFlipCanvasOnPaintForRTLUI(false);
    SetFocusBehavior(FocusBehavior::NEVER);
  }
  TuckHandle(const TuckHandle&) = delete;
  TuckHandle& operator=(const TuckHandle&) = delete;
  ~TuckHandle() override = default;

  // views::ImageButton:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SetBackground(views::CreateSolidBackground(ColorUtil::GetSecondToneColor(
        DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
            ? SK_ColorWHITE
            : SK_ColorBLACK)));
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(*icon_, kTuckHandleIconSize, SK_ColorWHITE));
  }

 private:
  // The untuck icon.
  const gfx::VectorIcon* const icon_;
};

// Scoped class which makes modifications while a window is tucked. It owns a
// tuck handle widget that will bring the hidden window back onscreen.
class FloatController::ScopedWindowTucker {
 public:
  explicit ScopedWindowTucker(aura::Window* window) : window_(window) {
    DCHECK(window_);
  }
  ScopedWindowTucker(const ScopedWindowTucker&) = delete;
  ScopedWindowTucker& operator=(const ScopedWindowTucker&) = delete;
  ~ScopedWindowTucker() = default;

  views::Widget* tuck_handle_widget() { return tuck_handle_widget_.get(); }

  void ShowTuckHandle(const MagnetismCorner magnetism_corner) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.activatable = views::Widget::InitParams::Activatable::kYes;
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.parent =
        window_->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
    params.init_properties_container.SetProperty(kHideInOverviewKey, true);
    params.init_properties_container.SetProperty(kForceVisibleInMiniViewKey,
                                                 false);
    params.name = "TuckHandleWidget";
    tuck_handle_widget_->Init(std::move(params));

    // The window should already be tucked offscreen.
    gfx::Point tuck_handle_origin = window_->GetTargetBounds().left_center();
    bool left = true;
    switch (magnetism_corner) {
      case MagnetismCorner::kTopLeft:
      case MagnetismCorner::kBottomLeft:
        tuck_handle_origin = window_->GetTargetBounds().right_center() -
                             gfx::Vector2d(0, kTuckHandleHeight / 2);

        break;
      case MagnetismCorner::kTopRight:
      case MagnetismCorner::kBottomRight:
        tuck_handle_origin =
            window_->GetTargetBounds().left_center() -
            gfx::Vector2d(kTuckHandleWidth, kTuckHandleHeight / 2);
        left = false;
        break;
    }
    tuck_handle_widget_->SetContentsView(std::make_unique<TuckHandle>(
        base::BindRepeating(&ScopedWindowTucker::OnButtonPressed,
                            base::Unretained(this)),
        left ? &kKsvArrowRightIcon : &kKsvArrowLeftIcon));
    ui::Layer* layer = tuck_handle_widget_->GetLayer();
    layer->SetFillsBoundsOpaquely(false);
    layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    if (left) {
      layer->SetRoundedCornerRadius(
          {0, kTuckHandleCornerRadius, kTuckHandleCornerRadius, 0});
    } else {
      layer->SetRoundedCornerRadius(
          {kTuckHandleCornerRadius, 0, 0, kTuckHandleCornerRadius});
    }
    tuck_handle_widget_->Show();
    tuck_handle_widget_->SetBounds(gfx::Rect(
        tuck_handle_origin, gfx::Size(kTuckHandleWidth, kTuckHandleHeight)));
  }

  void OnButtonPressed() {
    // Untuck the window, which sets the window bounds back onscreen.
    // Destroys `this`.
    Shell::Get()->float_controller()->MaybeUntuckFloatedWindowForTablet(
        window_);
  }

 private:
  // The window that is being tucked. Will be tucked and untucked by the tuck
  // handle.
  aura::Window* window_;

  views::UniqueWidgetPtr tuck_handle_widget_ =
      std::make_unique<views::Widget>();
};

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
  }

  FloatedWindowInfo(const FloatedWindowInfo&) = delete;
  FloatedWindowInfo& operator=(const FloatedWindowInfo&) = delete;
  ~FloatedWindowInfo() override {
    // Reset the window position auto-managed status if it was auto managed.
    if (was_position_auto_managed_) {
      WindowState::Get(floated_window_)->SetWindowPositionManaged(true);
    }
  }

  const Desk* desk() const { return desk_; }
  void set_desk(const Desk* desk) { desk_ = desk; }

  bool is_tucked_for_tablet() const { return !!scoped_window_tucker_; }

  MagnetismCorner magnetism_corner() const { return magnetism_corner_; }
  void set_magnetism_corner(MagnetismCorner magnetism_corner) {
    magnetism_corner_ = magnetism_corner;
  }

  void MaybeTuckWindow() {
    scoped_window_tucker_ =
        std::make_unique<ScopedWindowTucker>(floated_window_);

    UpdateWindowBoundsForTablet(floated_window_);

    // Must be called after the tucked window bounds are updated, to align the
    // handle with the window.
    scoped_window_tucker_->ShowTuckHandle(magnetism_corner_);
  }

  void MaybeUntuckWindow() { scoped_window_tucker_.reset(); }

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

 private:
  // The `floated_window` this object is hosting information for.
  aura::Window* floated_window_;

  // When a window is floated, the window position should not be auto-managed.
  // Use this value to reset the auto-managed state when unfloating a window.
  const bool was_position_auto_managed_;

  // Scoped object that handles the special tucked window state, which is not a
  // normal window state. Null when  `floated_window_`  is currently not tucked.
  std::unique_ptr<ScopedWindowTucker> scoped_window_tucker_;

  // The desk where floated window belongs to.
  // When a window is getting floated, it moves from desk container to float
  // container, this Desk pointer is used to determine floating window's desk
  // ownership, since floated window should only be shown on the desk it belongs
  // to.
  const Desk* desk_;

  // The corner the `floated_window_` should be magnetized to.
  // By default it magnetizes to the bottom right when first floated.
  MagnetismCorner magnetism_corner_ = MagnetismCorner::kBottomRight;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      floated_window_observation_{this};
};

// -----------------------------------------------------------------------------
// FloatController:

FloatController::FloatController() {
  shell_observation_.Observe(Shell::Get());
  for (aura::Window* root : Shell::GetAllRootWindows())
    OnRootWindowAdded(root);
}

FloatController::~FloatController() = default;

// static
gfx::Rect FloatController::GetPreferredFloatWindowClamshellBounds(
    aura::Window* window) {
  DCHECK(chromeos::wm::CanFloatWindow(window));
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

  const int padding_dp = chromeos::wm::kFloatedWindowPaddingDp;
  const int preferred_width =
      std::min(preferred_bounds.width(), work_area.width() - 2 * padding_dp);
  const int preferred_height =
      std::min(preferred_bounds.height(), work_area.height() - 2 * padding_dp);

  return gfx::Rect(work_area.width() - preferred_width - padding_dp,
                   work_area.height() - preferred_height - padding_dp,
                   preferred_width, preferred_height);
}

gfx::Rect FloatController::GetPreferredFloatWindowTabletBounds(
    aura::Window* floated_window) const {
  const gfx::Rect work_area =
      WorkAreaInsets::ForWindow(floated_window->GetRootWindow())
          ->user_work_area_bounds();
  const bool landscape =
      chromeos::wm::IsLandscapeOrientationForWindow(floated_window);
  const gfx::Size preferred_size =
      chromeos::wm::GetPreferredFloatedWindowTabletSize(work_area, landscape);
  const gfx::Size minimum_size = floated_window->delegate()->GetMinimumSize();

  const int width = std::max(preferred_size.width(), minimum_size.width());

  // Preferred height is always greater than minimum height since this function
  // won't be called otherwise.
  DCHECK_GT(preferred_size.height(), minimum_size.height());
  const int height = preferred_size.height();

  // Get `floated_window_info` from `floated_window_info_map_`, `window` must be
  // floated before calling into this function.
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);

  // Update the origin of the floated window based on whichever corner it is
  // magnetized to.
  gfx::Point origin;

  const MagnetismCorner magnetism_corner =
      floated_window_info->magnetism_corner();
  const int padding_dp = chromeos::wm::kFloatedWindowPaddingDp;
  switch (magnetism_corner) {
    case MagnetismCorner::kTopLeft:
      origin = gfx::Point(padding_dp, padding_dp);
      break;
    case MagnetismCorner::kTopRight:
      origin = gfx::Point(work_area.right() - width - padding_dp, padding_dp);
      break;
    case MagnetismCorner::kBottomLeft:
      origin = gfx::Point(padding_dp, work_area.bottom() - height - padding_dp);
      break;
    case MagnetismCorner::kBottomRight:
      origin = gfx::Point(work_area.right() - width - padding_dp,
                          work_area.bottom() - height - padding_dp);
      break;
  }

  // If the window is tucked, shift it so the window is offscreen.
  if (floated_window_info->is_tucked_for_tablet()) {
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
  UpdateWindowBoundsForTablet(floated_window);
}

bool FloatController::IsFloatedWindowTuckedForTablet(
    const aura::Window* floated_window) const {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  return floated_window_info->is_tucked_for_tablet();
}

views::Widget* FloatController::GetTuckHandleWidget(
    const aura::Window* floated_window) const {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  return floated_window_info->GetTuckHandleWidget();
}

void FloatController::OnDragCompletedForTablet(
    aura::Window* floated_window,
    const gfx::PointF& last_location_in_parent) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);

  // Use the display bounds since the user may drag on to the shelf or spoken
  // feedback bar.
  const gfx::RectF display_bounds(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(floated_window->GetRootWindow())
          .bounds());

  // Check which corner to magnetize to based on which quadrent of the display
  // the mouse/touch was released. If it somehow falls outside, then magnetize
  // to the previous location.
  gfx::RectF display_bounds_left, display_bounds_right;
  display_bounds.SplitVertically(&display_bounds_left, &display_bounds_right);
  const float center_y = display_bounds.CenterPoint().y();
  MagnetismCorner magnetism_corner = floated_window_info->magnetism_corner();
  if (display_bounds_left.InclusiveContains(last_location_in_parent)) {
    magnetism_corner = last_location_in_parent.y() < center_y
                           ? MagnetismCorner::kTopLeft
                           : MagnetismCorner::kBottomLeft;
  } else if (display_bounds_right.InclusiveContains(last_location_in_parent)) {
    magnetism_corner = last_location_in_parent.y() < center_y
                           ? MagnetismCorner::kTopRight
                           : MagnetismCorner::kBottomRight;
  }
  floated_window_info->set_magnetism_corner(magnetism_corner);
  UpdateWindowBoundsForTablet(floated_window);
}

void FloatController::OnFlingOrSwipeForTablet(aura::Window* floated_window,
                                              bool left,
                                              bool up) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  MagnetismCorner magnetism_corner;
  if (left && up) {
    magnetism_corner = MagnetismCorner::kTopLeft;
  } else if (left && !up) {
    magnetism_corner = MagnetismCorner::kBottomLeft;
  } else if (!left && up) {
    magnetism_corner = MagnetismCorner::kTopRight;
  } else {
    DCHECK(!left && !up);
    magnetism_corner = MagnetismCorner::kBottomRight;
  }

  floated_window_info->set_magnetism_corner(magnetism_corner);
  floated_window_info->MaybeTuckWindow();
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
  float_info->set_desk(target_desk);
  if (root != target_root) {
    // If `floated_window_` is dragged to a desk on a different display, we
    // also need to move it to the target display.
    window_util::MoveWindowToDisplay(floated_window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(target_root)
                                         .id());
  }

  // Hide `floated_window` since it's been moved to an inactive desk.
  HideFloatedWindow(floated_window);
}

void FloatController::OnTabletModeStarting() {
  DCHECK(!floated_window_info_map_.empty());
  // Temporary vector here to avoid mutating the map while iterating it.
  std::vector<aura::Window*> windows_need_reset;
  for (auto& [window, info] : floated_window_info_map_) {
    if (!chromeos::wm::CanFloatWindow(window))
      windows_need_reset.push_back(window);
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
  if (chromeos::TabletState::Get()->state() !=
      display::TabletState::kInTabletMode) {
    return;
  }

  if ((display::DisplayObserver::DISPLAY_METRIC_WORK_AREA & metrics) == 0)
    return;

  DCHECK(!floated_window_info_map_.empty());
  std::vector<aura::Window*> windows_need_reset;
  for (auto& [window, info] : floated_window_info_map_) {
    if (!chromeos::wm::CanFloatWindow(window))
      windows_need_reset.push_back(window);
    else
      UpdateWindowBoundsForTablet(window);
  }
  for (auto* window : windows_need_reset)
    ResetFloatedWindow(window);
}

void FloatController::OnRootWindowAdded(aura::Window* root_window) {
  workspace_event_handlers_[root_window] =
      std::make_unique<WorkspaceEventHandler>(
          root_window->GetChildById(kShellWindowId_FloatContainer));
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
  // Get the active desk where the window belongs to before moving it to float
  // container.
  DCHECK(desks_util::BelongsToActiveDesk(window));
  const Desk* desk = desk_controller->GetTargetActiveDesk();
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