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
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/tablet_mode_tuck_education.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/scoped_window_tucker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_default_layout_manager.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/constants.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/scoped_animation_disabler.h"

namespace ash {

namespace {

// The ideal dimensions of a clamshell floated window before factoring in its
// minimum size (if any) is the available work area multiplied by these ratios.
constexpr float kFloatedWindowClamshellWidthRatio = 1.f / 3.f;
constexpr float kFloatedWindowClamshellHeightRatio = 0.7f;

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
  if (window_state->is_client_controlled()) {
    // If any animation is requested, it will directly animate the
    // client-controlled windows for a rich animation. The client bounds change
    // will follow.
    if (animation_type != WindowState::BoundsChangeAnimationType::kNone) {
      TabletModeWindowState::UpdateWindowPosition(window_state, animation_type);
    }
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
  wm::ScopedAnimationDisabler disabler(floated_window);
  floated_window->Hide();
}

// Shows the given floated window.
void ShowFloatedWindow(aura::Window* floated_window) {
  DCHECK(floated_window);
  if (floated_window->IsVisible()) {
    return;
  }

  wm::ScopedAnimationDisabler disabler(floated_window);
  floated_window->Show();
}

gfx::Rect GetFloatBounds(const gfx::Size& size,
                         const gfx::Rect& work_area_bounds,
                         chromeos::FloatStartLocation location) {
  const int padding_dp = chromeos::wm::kFloatedWindowPaddingDp;
  int origin_x;
  const int origin_y = work_area_bounds.bottom() - size.height() - padding_dp;
  switch (location) {
    case chromeos::FloatStartLocation::kBottomLeft: {
      origin_x = padding_dp;
      break;
    }
    case chromeos::FloatStartLocation::kBottomRight: {
      origin_x = work_area_bounds.right() - size.width() - padding_dp;
      break;
    }
  }
  return gfx::Rect(gfx::Point(origin_x, origin_y), size);
}

class FloatLayoutManager : public WmDefaultLayoutManager {
 public:
  FloatLayoutManager() = default;
  FloatLayoutManager(const FloatLayoutManager&) = delete;
  FloatLayoutManager& operator=(const FloatLayoutManager&) = delete;
  ~FloatLayoutManager() override = default;

  // WmDefaultLayoutManager:
  void OnWindowAddedToLayout(aura::Window* child) override {
    // We don't support multiple displays in tablet mode, so this function is
    // called when the window is moved into the float container from a desk
    // container. This happens during a state change, and we can let the
    // transition event handle setting the floated window bounds instead.
    if (Shell::Get()->IsInTabletMode()) {
      return;
    }

    WindowState* window_state = WindowState::Get(child);
    WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
    window_state->OnWMEvent(&event);
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {
    // Same as what we are doing inside
    // `WorkspaceLayoutManager::OnWillRemoveWindowFromLayout` for this. But we
    // need to do it separately here as `WorkspaceLayoutManager` is not tracking
    // the float container.
    WindowState::Get(child)->set_pre_added_to_workspace_window_bounds(
        child->bounds());
  }

  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    // This should result in sending a bounds change WMEvent to properly support
    // client-controlled windows (e.g. ARC++).
    WindowState* window_state = WindowState::Get(child);
    SetBoundsWMEvent event(requested_bounds);
    window_state->OnWMEvent(&event);
  }
};

class FloatScopedWindowTuckerDelegate : public ScopedWindowTucker::Delegate {
 public:
  FloatScopedWindowTuckerDelegate() = default;
  FloatScopedWindowTuckerDelegate(const FloatScopedWindowTuckerDelegate&) =
      delete;
  FloatScopedWindowTuckerDelegate& operator=(
      const FloatScopedWindowTuckerDelegate&) = delete;
  ~FloatScopedWindowTuckerDelegate() override = default;

  void PaintTuckHandle(gfx::Canvas* canvas, int width, bool left) override {
    // Flip the canvas horizontally for `left` tuck handle.
    if (left) {
      canvas->Translate(gfx::Vector2d(width, 0));
      canvas->Scale(-1, 1);
    }

    // We draw three icons on top of each other because we need separate
    // themeing on different parts which is not supported by `VectorIcon`.
    const bool dark_mode =
        DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();

    // Paint the container bottom layer with default 80% opacity.
    SkColor color = dark_mode ? gfx::kGoogleGrey500 : gfx::kGoogleGrey600;
    const SkColor bottom_color =
        SkColorSetA(color, std::round(SkColorGetA(color) * 0.8f));

    const gfx::ImageSkia& tuck_container_bottom = gfx::CreateVectorIcon(
        kTuckHandleContainerBottomIcon, ScopedWindowTucker::kTuckHandleWidth,
        bottom_color);
    canvas->DrawImageInt(tuck_container_bottom, 0, 0);

    // Paint the container top layer. This is mostly transparent, with 12%
    // opacity.
    color = dark_mode ? gfx::kGoogleGrey200 : gfx::kGoogleGrey600;
    const SkColor top_color =
        SkColorSetA(color, std::round(SkColorGetA(color) * 0.12f));
    const gfx::ImageSkia& tuck_container_top =
        gfx::CreateVectorIcon(kTuckHandleContainerTopIcon,
                              ScopedWindowTucker::kTuckHandleWidth, top_color);
    canvas->DrawImageInt(tuck_container_top, 0, 0);

    const gfx::ImageSkia& tuck_icon = gfx::CreateVectorIcon(
        kTuckHandleChevronIcon, ScopedWindowTucker::kTuckHandleWidth,
        SK_ColorWHITE);
    canvas->DrawImageInt(tuck_icon, 0, 0);
  }

  int ParentContainerId() const override {
    return kShellWindowId_FloatContainer;
  }

  void UpdateWindowPosition(aura::Window* window, bool left) override {
    TabletModeWindowState::UpdateWindowPosition(
        WindowState::Get(window),
        WindowState::BoundsChangeAnimationType::kNone);
  }

  void UntuckWindow(aura::Window* window) override {
    Shell::Get()->float_controller()->MaybeUntuckFloatedWindowForTablet(window);
  }

  void OnAnimateTuckEnded(aura::Window* window) override {
    wm::ScopedAnimationDisabler disable(window);
    window->Hide();
  }

  gfx::Rect GetTuckHandleBounds(bool left,
                                const gfx::Rect& window_bounds) const override {
    const gfx::Point tuck_handle_origin =
        left ? window_bounds.right_center() -
                   gfx::Vector2d(0, ScopedWindowTucker::kTuckHandleHeight / 2)
             : window_bounds.left_center() -
                   gfx::Vector2d(ScopedWindowTucker::kTuckHandleWidth,
                                 ScopedWindowTucker::kTuckHandleHeight / 2);
    return gfx::Rect(tuck_handle_origin,
                     gfx::Size(ScopedWindowTucker::kTuckHandleWidth,
                               ScopedWindowTucker::kTuckHandleHeight));
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// FloatedWindowInfo:

// Represents and stores information used for window's floated state.
class FloatController::FloatedWindowInfo : public aura::WindowObserver,
                                           public views::WidgetObserver {
 public:
  FloatedWindowInfo(aura::Window* floated_window, const Desk* desk)
      : floated_window_(floated_window),
        was_position_auto_managed_(
            DisableAndGetOriginalPositionAutoManaged(floated_window)),
        desk_(desk) {
    DCHECK(floated_window_);
    floated_window_observation_.Observe(floated_window);
    if (auto* widget =
            views::Widget::GetWidgetForNativeWindow(floated_window)) {
      floated_widget_observation_.Observe(widget);
      last_minimum_size_ = widget->GetMinimumSize();
      last_maximum_size_ = widget->GetMaximumSize();
    }

    if (desk->is_active()) {
      float_start_time_ = base::TimeTicks::Now();
    }

    if (display::Screen::GetScreen()->InTabletMode() &&
        TabletModeTuckEducation::CanActivateTuckEducation() &&
        !Shell::Get()
             ->float_controller()
             ->disable_tuck_education_for_testing_) {
      tuck_education_ =
          std::make_unique<TabletModeTuckEducation>(floated_window);
    }
  }

  FloatedWindowInfo(const FloatedWindowInfo&) = delete;
  FloatedWindowInfo& operator=(const FloatedWindowInfo&) = delete;
  ~FloatedWindowInfo() override {
    // Reset the window position auto-managed status if it was auto managed.
    if (was_position_auto_managed_) {
      WindowState::Get(floated_window_)->SetWindowPositionManaged(true);
    }
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
    scoped_window_tucker_ = std::make_unique<ScopedWindowTucker>(
        std::make_unique<FloatScopedWindowTuckerDelegate>(), floated_window_,
        left);
    scoped_window_tucker_->AnimateTuck();

    // Education doesn't need to happen after the user has successfully tucked
    // once.
    TabletModeTuckEducation::OnWindowTucked();
  }

  void OnUntuckAnimationEnded() {
    scoped_window_tucker_.reset();

    // No-op for non-client-controlled windows. For the client-controlled
    // windows, this ensures the bounds is sync between Chrome and the client.
    // We don't send the offscreen bounds to the client when tucked, so we need
    // to send the proper floated bounds when untucked.
    UpdateWindowBoundsForTablet(floated_window_,
                                WindowState::BoundsChangeAnimationType::kNone);
  }

  void MaybeUntuckWindow(bool animate) {
    // The order here matters: `is_tucked_for_tablet_` must be set to false
    // before `TabletModeWindowState::UpdateWindowPosition()` or
    // `AnimateUntuck()` gets the untucked window bounds.
    is_tucked_for_tablet_ = false;

    if (!animate) {
      scoped_window_tucker_.reset();
      UpdateWindowBoundsForTablet(
          floated_window_, WindowState::BoundsChangeAnimationType::kNone);
      return;
    }

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
    DCHECK(
        floated_window_observation_.IsObservingSource(floated_window_.get()));
    // Note that `this` is deleted below in `OnFloatedWindowDestroying()` and
    // should not be accessed after this.
    Shell::Get()->float_controller()->OnFloatedWindowDestroying(window);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window != floated_window_) {
      return;
    }

    // When a floated window switches desks, it is hidden or shown. We track the
    // amount of time a floated window is visible on the active desk to avoid
    // recording the cases if a floated window is floated indefinitely on an
    // inactive desk. Check if the desk is active as well, as some UI such as
    // the saved desks library view may temporarily hide the floated window on
    // the active desk.
    if (visible && desk_->is_active()) {
      if (float_start_time_.is_null()) {
        float_start_time_ = base::TimeTicks::Now();
      }
      return;
    }

    if (!visible && !desk_->is_active()) {
      MaybeRecordFloatWindowDuration();
    }
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    CHECK_EQ(floated_window_, window);

    if (key == aura::client::kWindowWorkspaceKey &&
        desks_util::IsZOrderTracked(window)) {
      auto* desks_controller = Shell::Get()->desks_controller();
      if (desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
        desks_controller->AddVisibleOnAllDesksWindow(window);
      } else {
        desks_controller->MaybeRemoveVisibleOnAllDesksWindow(window);
      }

      return;
    }

    // Always on top window cannot be floated, so if a floated window becomes
    // always on top, exit float state.
    if (key == aura::client::kZOrderingKey) {
      if (window->GetProperty(aura::client::kZOrderingKey) !=
          ui::ZOrderLevel::kNormal) {
        // Destroys `this`.
        Shell::Get()->float_controller()->ResetFloatedWindow(floated_window_);
      }
      return;
    }

    if (key == aura::client::kResizeBehaviorKey &&
        static_cast<int>(old) !=
            window->GetProperty(aura::client::kResizeBehaviorKey)) {
      OnResizabilityOrSizeConstraintsChanged();
    }
  }

  // views::Widget::WidgetObserver:
  void OnWidgetSizeConstraintsChanged(views::Widget* widget) override {
    CHECK_EQ(views::Widget::GetWidgetForNativeWindow(floated_window_), widget);

    if (last_minimum_size_ != widget->GetMinimumSize() ||
        last_maximum_size_ != widget->GetMaximumSize()) {
      OnResizabilityOrSizeConstraintsChanged();
      last_minimum_size_ = widget->GetMinimumSize();
      last_maximum_size_ = widget->GetMaximumSize();
    }
  }

 private:
  // Called when the floated window's resizability or size constraints changed.
  void OnResizabilityOrSizeConstraintsChanged() {
    // If `window` is in transitional snapped state, `window` is going to be
    // snapped very soon so we don't need to apply the float bounds policies.
    // Otherwise, the bounds change request may be queued and applied after
    // `window` is snapped.
    if (SplitViewController::Get(floated_window_)
            ->IsWindowInTransitionalState(floated_window_)) {
      return;
    }

    // The minimum size could change and as a result, the floated window might
    // not be floatable anymore. In this case, unfloat it.
    if (!chromeos::wm::CanFloatWindow(floated_window_)) {
      Shell::Get()->float_controller()->ResetFloatedWindow(floated_window_);
      return;
    }

    if (Shell::Get()->IsInTabletMode()) {
      UpdateWindowBoundsForTablet(
          floated_window_, WindowState::BoundsChangeAnimationType::kNone);
    }
  }

  // The `floated_window` this object is hosting information for.
  raw_ptr<aura::Window> floated_window_;

  // When a window is floated, the window position should not be auto-managed.
  // Use this value to reset the auto-managed state when unfloating a window.
  const bool was_position_auto_managed_;

  // Scoped object that handles the special tucked window state, which is not
  // a normal window state. Null when `floated_window_` is currently not tucked.
  std::unique_ptr<ScopedWindowTucker> scoped_window_tucker_;

  // An object responsible for managing the tuck education nudge and animations.
  std::unique_ptr<TabletModeTuckEducation> tuck_education_;

  // Used to get the tucked window bounds (as opposed to normal floated). False
  // during `scoped_window_tucker_` construction.
  bool is_tucked_for_tablet_ = false;

  // The desk where floated window belongs to.
  // When a window is getting floated, it moves from desk container to float
  // container, this Desk pointer is used to determine floating window's desk
  // ownership, since floated window should only be shown on the desk it belongs
  // to.
  raw_ptr<const Desk, DanglingUntriaged> desk_;

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

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      floated_widget_observation_{this};

  gfx::Size last_minimum_size_;
  gfx::Size last_maximum_size_;

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
gfx::Rect FloatController::GetFloatWindowClamshellBounds(
    aura::Window* window,
    chromeos::FloatStartLocation location) {
  DCHECK(chromeos::wm::CanFloatWindow(window));

  // In the case of window restore, as we re-float previously floated window, we
  // will use `window->bounds()`to restore floated window's previous
  // location.
  if (window->GetProperty(app_restore::kLaunchedFromAppRestoreKey)) {
    return window->bounds();
  }

  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window);

  const int padding_dp = chromeos::wm::kFloatedWindowPaddingDp;

  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0) {
    // Unresizable windows must not be resized for any reason.
    return GetFloatBounds(window->bounds().size(), work_area, location);
  }

  // Default float size is 1/3 width and 70% height of `work_area`.
  // Float bounds also should not be smaller than min bounds, use min
  // width/height if it exceeds the limit.
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  gfx::Rect preferred_bounds =
      gfx::Rect(std::max(static_cast<int>(work_area.width() *
                                          kFloatedWindowClamshellWidthRatio),
                         minimum_size.width()),
                std::max(static_cast<int>(work_area.height() *
                                          kFloatedWindowClamshellHeightRatio),
                         minimum_size.height()));

  // If user has already adjusted the window to be a size smaller than the
  // calculated preferred size, use user size instead.
  if (window->bounds().height() <= preferred_bounds.height() &&
      window->bounds().width() <= preferred_bounds.width()) {
    preferred_bounds = window->bounds();
  }

  const int preferred_width =
      std::min(preferred_bounds.width(), work_area.width() - 2 * padding_dp);
  const int preferred_height =
      std::min(preferred_bounds.height(), work_area.height() - 2 * padding_dp);
  return GetFloatBounds(gfx::Size(preferred_width, preferred_height), work_area,
                        location);
}

// static
gfx::Rect FloatController::GetFloatWindowTabletBounds(aura::Window* window) {
  const gfx::Size preferred_size =
      chromeos::wm::GetFloatedWindowTabletSize(window);

  const int width = preferred_size.width();
  const int height = preferred_size.height();

  // Get `floated_window_info` from the float controller. For non
  // client-controlled apps, it is expected we call this function on already
  // floated windows. For client controlled windows, we need to send the floated
  // bounds before the client applies the float state, which results in using
  // `GetFloatWindowTabletBounds` before `FloatImpl` is called.
  auto* floated_window_info =
      Shell::Get()->float_controller()->MaybeGetFloatedWindowInfo(window);
#if DCHECK_IS_ON()
  if (!WindowState::Get(window)->is_client_controlled()) {
    DCHECK(floated_window_info);
  }
#endif

  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window);

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

void FloatController::ToggleFloat(aura::Window* window) {
  if (WindowState::Get(window)->IsFloated()) {
    UnsetFloat(window);
  } else {
    SetFloat(window, chromeos::FloatStartLocation::kBottomRight);
  }
}

void FloatController::MaybeUntuckFloatedWindowForTablet(
    aura::Window* floated_window) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(floated_window);
  DCHECK(floated_window_info);
  floated_window_info->MaybeUntuckWindow(/*animate=*/true);
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
  floated_window_info->set_magnetism_corner(
      GetMagnetismCornerForBounds(floated_window->GetBoundsInScreen()));
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

  if (!desks_util::IsWindowVisibleOnAllWorkspaces(floated_window)) {
    // Update `floated_window` visibility based on target desk's activation
    // status.
    if (target_desk->is_active()) {
      ShowFloatedWindow(floated_window);
    } else {
      HideFloatedWindow(floated_window);
    }
  }

  active_desk->NotifyContentChanged();
  target_desk->NotifyContentChanged();
}

void FloatController::ClearWorkspaceEventHandler(aura::Window* root) {
  workspace_event_handlers_.erase(root);
}

void FloatController::OnDeskActivationChanged(const Desk* activated,
                                              const Desk* deactivated) {
  // Since floated windows are not children of desk containers, switching desks
  // (which changes the visibility of desks' containers) won't automatically
  // update the floated windows' visibility. Therefore, here we hide the floated
  // window belonging to the deactivated desk, and show the one belonging to the
  // activated desk.
  auto deactivated_desk_floated_window_info_iter = base::ranges::find_if(
      floated_window_info_map_, [deactivated](const auto& floated_window_info) {
        return floated_window_info.second->desk() == deactivated;
      });
  if (deactivated_desk_floated_window_info_iter !=
      floated_window_info_map_.end()) {
    // If we are currently not in tablet mode, no need to untuck, which would
    // update the window bounds.
    if (Shell::Get()->IsInTabletMode()) {
      deactivated_desk_floated_window_info_iter->second->MaybeUntuckWindow(
          /*animate=*/false);
    }
    HideFloatedWindow(deactivated_desk_floated_window_info_iter->first);
  }

  if (auto* activated_desk_floated_window =
          FindFloatedWindowOfDesk(activated)) {
    ShowFloatedWindow(activated_desk_floated_window);
    // Activate the floated window if it is the top window. This is normally
    // done in `Desk::Activate`, but floated windows are technically not owned
    // by the desk, and the window is still hidden at that point so it isn't in
    // the MRU list.
    if (auto* top_window = window_util::GetTopWindow();
        top_window == activated_desk_floated_window) {
      wm::ActivateWindow(top_window);
    }
  }
}

void FloatController::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  // The work area can change while entering or exiting tablet mode. The float
  // window changes related with those changes are handled in
  // `OnTabletModeStarting`, `OnTabletModeEnding` or attaching/detaching window
  // states.
  display::TabletState tablet_state =
      display::Screen::GetScreen()->GetTabletState();
  if (tablet_state == display::TabletState::kEnteringTabletMode ||
      tablet_state == display::TabletState::kExitingTabletMode) {
    return;
  }

  const uint32_t filter = DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_WORK_AREA;
  if ((filter & metrics) == 0) {
    return;
  }

  DCHECK(!floated_window_info_map_.empty());
  std::vector<aura::Window*> windows_need_reset;
  for (auto& [window, info] : floated_window_info_map_) {
    if (!chromeos::wm::CanFloatWindow(window)) {
      windows_need_reset.push_back(window);
    } else {
      // Let the state object handle the display change. This is normally
      // handled by the `WorkspaceLayoutManager`, but the float container does
      // not have one attached.
      if (metrics & DISPLAY_METRIC_BOUNDS ||
          metrics & DISPLAY_METRIC_WORK_AREA) {
        const DisplayMetricsChangedWMEvent wm_event(metrics);
        WindowState::Get(window)->OnWMEvent(&wm_event);
      }
    }
  }
  for (auto* window : windows_need_reset)
    ResetFloatedWindow(window);

  // Do not observe the animator in `OnRootWindowAdded` because there is an
  // unittest that overwrites the animator for the root window just before
  // running the animation.
  if (DISPLAY_METRIC_ROTATION & metrics) {
    if (auto* root_controller =
            Shell::GetRootWindowControllerWithDisplayId(display.id())) {
      if (auto* animator = root_controller->GetScreenRotationAnimator();
          animator &&
          !screen_rotation_observations_.IsObservingSource(animator)) {
        screen_rotation_observations_.AddObservation(animator);
      }
    }
  }
}

void FloatController::OnDisplayTabletStateChanged(display::TabletState state) {
  switch (state) {
    case display::TabletState::kInClamshellMode:
    case display::TabletState::kEnteringTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      OnTabletModeStarted();
      break;
    case display::TabletState::kExitingTabletMode:
      OnTabletModeEnding();
      break;
  }
}

void FloatController::OnRootWindowAdded(aura::Window* root_window) {
  workspace_event_handlers_[root_window] =
      std::make_unique<WorkspaceEventHandler>(
          root_window->GetChildById(kShellWindowId_FloatContainer));
  root_window->GetChildById(kShellWindowId_FloatContainer)
      ->SetLayoutManager(std::make_unique<FloatLayoutManager>());
}

void FloatController::OnRootWindowWillShutdown(aura::Window* root_window) {
  if (auto* const animator = RootWindowController::ForWindow(root_window)
                                 ->GetScreenRotationAnimator();
      animator && screen_rotation_observations_.IsObservingSource(animator)) {
    screen_rotation_observations_.RemoveObservation(animator);
  }
}

void FloatController::OnScreenCopiedBeforeRotation() {}

void FloatController::OnScreenRotationAnimationFinished(
    ScreenRotationAnimator* animator,
    bool canceled) {
  // Re-send the correct floated bounds here. ARC sometimes overwrites the
  // floated bounds against the new bounds during the rotation animation.
  // TODO(b/278519956): Remove this workaround once ARC/Exo handle rotation
  // bounds better.
  for (auto& [window, info] : floated_window_info_map_) {
    if (WindowState::Get(window)->is_client_controlled()) {
      const gfx::Rect bounds =
          display::Screen::GetScreen()->InTabletMode()
              ? GetFloatWindowTabletBounds(window)
              : GetFloatWindowClamshellBounds(
                    window, chromeos::FloatStartLocation::kBottomRight);
      const SetBoundsWMEvent event(bounds);
      WindowState::Get(window)->OnWMEvent(&event);

      // When a window is tucked, ash has full control over the bounds.
      if (IsFloatedWindowTuckedForTablet(window)) {
        TabletModeWindowState::UpdateWindowPosition(
            WindowState::Get(window),
            WindowState::BoundsChangeAnimationType::kNone);
      }
    }
  }
}

void FloatController::OnPinnedStateChanged(aura::Window* pinned_window) {
  if (aura::Window* floated_window =
          window_util::GetFloatedWindowForActiveDesk()) {
    // Note that the `pinned_window` will still not be null when unpinning.
    // Check the screen pinning controller for the to be pinned window.
    if (aura::Window* to_be_pinned_window =
            Shell::Get()->screen_pinning_controller()->pinned_window()) {
      if (to_be_pinned_window != floated_window) {
        HideFloatedWindow(floated_window);
      }
    } else {
      ShowFloatedWindow(floated_window);
    }
  }
}

void FloatController::SetFloat(
    aura::Window* window,
    chromeos::FloatStartLocation float_start_location) {
  auto* window_state = WindowState::Get(window);
  if (!window_state->IsFloated()) {
    const WindowFloatWMEvent float_event(float_start_location);
    window_state->OnWMEvent(&float_event);
  }
}

void FloatController::UnsetFloat(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  if (window_state->IsFloated()) {
    const WMEvent restore_event(WM_EVENT_RESTORE);
    window_state->OnWMEvent(&restore_event);
  }
}

// static
FloatController::MagnetismCorner FloatController::GetMagnetismCornerForBounds(
    const gfx::Rect& bounds_in_screen) {
  // Check which corner to magnetize to based on which quadrant of the display
  // the centerpoint of the window was on touch released. Note that the
  // centerpoint may be offscreen.
  const gfx::Point display_bounds_center =
      display::Screen::GetScreen()
          ->GetDisplayMatching(bounds_in_screen)
          .bounds()
          .CenterPoint();
  const gfx::Point center_point = bounds_in_screen.CenterPoint();
  const bool is_left_half = center_point.x() < display_bounds_center.x();
  if (center_point.y() < display_bounds_center.y()) {
    // Top half.
    return is_left_half ? FloatController::MagnetismCorner::kTopLeft
                        : FloatController::MagnetismCorner::kTopRight;
  }
  // Bottom half.
  return is_left_half ? FloatController::MagnetismCorner::kBottomLeft
                      : FloatController::MagnetismCorner::kBottomRight;
}

void FloatController::FloatForTablet(aura::Window* window,
                                     chromeos::WindowStateType old_state_type) {
  CHECK(Shell::Get()->IsInTabletMode());

  FloatImpl(window);

  // Update the magnetism if we are coming from a state that can restore to
  // float state, or from snap state. The bounds will be updated later based on
  // the magnetism and account for work area.
  std::optional<MagnetismCorner> magnetism_corner;
  if (chromeos::IsMinimizedWindowStateType(old_state_type)) {
    magnetism_corner = GetMagnetismCornerForBounds(window->GetBoundsInScreen());
  } else if (chromeos::IsSnappedWindowStateType(old_state_type)) {
    // Update magnetism so that the float window is roughly in the same
    // location as it was when it was snapped.
    const bool left_or_top =
        old_state_type == chromeos::WindowStateType::kPrimarySnapped;
    const bool landscape = IsCurrentScreenOrientationLandscape();
    if (!left_or_top) {
      // Bottom or right snapped.
      magnetism_corner = MagnetismCorner::kBottomRight;
    } else if (landscape) {
      // Left snapped.
      magnetism_corner = MagnetismCorner::kBottomLeft;
    } else {
      CHECK(left_or_top && !landscape);
      // Top snapped.
      magnetism_corner = MagnetismCorner::kTopRight;
    }
  }

  if (!magnetism_corner) {
    return;
  }

  auto* floated_window_info = MaybeGetFloatedWindowInfo(window);
  CHECK(floated_window_info);
  floated_window_info->set_magnetism_corner(*magnetism_corner);
}

void FloatController::FloatImpl(aura::Window* window) {
  if (floated_window_info_map_.contains(window))
    return;

  auto* desk_controller = DesksController::Get();
  // Get the desk where the window belongs to before moving it to float
  // container.
  const Desk* desk = desks_util::GetDeskForContext(window);
  if (!desk) {
    return;
  }

  // If the window we want to float is already visible on all desks, then we
  // need to unfloat any other currently floated windows that exist on each
  // desk, as there should only be one floated window per desk.
  const bool reset_all_desks =
      desks_util::IsWindowVisibleOnAllWorkspaces(window);
  std::vector<aura::Window*> windows_to_reset;

  for (const auto& [floated_window, info] : floated_window_info_map_) {
    // Regardless if `window` is visible on all desks or not, if a floated
    // window already exists at the current desk, then we also want to unfloat
    // it.
    if (reset_all_desks || info->desk() == desk) {
      windows_to_reset.push_back(floated_window);
    }
  }

  // Since a floated window is always on top, we don't want to track its
  // z-ordering.
  if (reset_all_desks) {
    desk_controller->UntrackWindowFromAllDesks(window);
  }

  // Add floated window to `floated_window_info_map_`.
  // Note: this has to be called before `ResetFloatedWindow`. Because in the
  // call sequence of `ResetFloatedWindow` we will access
  // `floated_window_info_map_`, and hit a corner case where window
  // `IsFloated()` returns true, but `FindDeskOfFloatedWindow` returns nullptr.
  floated_window_info_map_.emplace(
      window, std::make_unique<FloatedWindowInfo>(window, desk));
  for (auto* reset_window : windows_to_reset) {
    ResetFloatedWindow(reset_window);
  }

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

  if (!desks_controller_observation_.IsObserving())
    desks_controller_observation_.Observe(desk_controller);
  if (!display_observer_)
    display_observer_.emplace(this);
}

void FloatController::UnfloatImpl(aura::Window* window) {
  auto* floated_window_info = MaybeGetFloatedWindowInfo(window);
  if (!floated_window_info)
    return;

  // Floated window have been hidden on purpose on the inactive desk.
  ShowFloatedWindow(window);
  // Re-parent window to the "parent" desk's desk container.
  floated_window_info->desk()
      ->GetDeskContainerForRoot(window->GetRootWindow())
      ->AddChild(window);
  floated_window_info_map_.erase(window);
  if (floated_window_info_map_.empty()) {
    desks_controller_observation_.Reset();
    display_observer_.reset();
  }

  // A floated window does not have per-desk z-order, so we need to start
  // tracking the window again after it is unfloated.
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    DesksController::Get()->TrackWindowOnAllDesks(window);
  }
}

void FloatController::ResetFloatedWindow(aura::Window* floated_window) {
  DCHECK(floated_window);
  DCHECK(WindowState::Get(floated_window)->IsFloated());
  UnsetFloat(floated_window);
}

FloatController::FloatedWindowInfo* FloatController::MaybeGetFloatedWindowInfo(
    const aura::Window* window) const {
  const auto iter = floated_window_info_map_.find(window);
  if (iter == floated_window_info_map_.end())
    return nullptr;
  return iter->second.get();
}

void FloatController::OnFloatedWindowDestroying(aura::Window* floated_window) {
  DesksController::Get()->MaybeRemoveVisibleOnAllDesksWindow(floated_window);

  floated_window_info_map_.erase(floated_window);
  if (floated_window_info_map_.empty()) {
    desks_controller_observation_.Reset();
    display_observer_.reset();
  }
}

void FloatController::OnTabletModeStarted() {
  DCHECK(!floated_window_info_map_.empty());
  // If a window can still remain floated, update its bounds, otherwise unfloat
  // it. Note that the bounds update has to happen after tablet mode has started
  // as opposed to while it is still starting, since some windows change their
  // minimum size, which tablet float bounds depend on.
  for (auto& [window, info] : floated_window_info_map_) {
    if (chromeos::wm::CanFloatWindow(window)) {
      info->set_magnetism_corner(
          GetMagnetismCornerForBounds(window->GetBoundsInScreen()));
      UpdateWindowBoundsForTablet(
          window, WindowState::BoundsChangeAnimationType::kCrossFade);
    } else {
      ResetFloatedWindow(window);
    }
  }
}

void FloatController::OnTabletModeEnding() {
  for (auto& [window, info] : floated_window_info_map_) {
    info->MaybeUntuckWindow(/*animate=*/false);
  }
}

}  // namespace ash
