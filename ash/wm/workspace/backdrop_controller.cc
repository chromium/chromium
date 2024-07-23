// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/backdrop_controller.h"

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/animation/animation_change_type.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// -----------------------------------------------------------------------------
// BackdropEventHandler:

class BackdropEventHandler : public ui::EventHandler {
 public:
  BackdropEventHandler() = default;

  BackdropEventHandler(const BackdropEventHandler&) = delete;
  BackdropEventHandler& operator=(const BackdropEventHandler&) = delete;

  ~BackdropEventHandler() override = default;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // If the event is targeted at the backdrop, it means the user has made an
    // interaction that is outside the window's bounds and we want to capture
    // it (usually when in spoken feedback mode). Handle the event (to prevent
    // behind-windows from receiving it) and play an earcon to notify the user.
    if (event->IsLocatedEvent()) {
      switch (event->type()) {
        case ui::EventType::kMousePressed:
        case ui::EventType::kMousewheel:
        case ui::EventType::kTouchPressed:
        case ui::EventType::kGestureBegin:
        case ui::EventType::kScroll:
        case ui::EventType::kScrollFlingStart:
          Shell::Get()->accessibility_controller()->PlayEarcon(
              Sound::kVolumeAdjust);
          break;
        default:
          break;
      }
      event->SetHandled();
    }
  }
};

// -----------------------------------------------------------------------------
// ScopedWindowVisibilityAnimationTypeResetter:

// Sets |window|'s visibility animation type to |new_type| and resets it back to
// WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT when it goes out of scope.
class ScopedWindowVisibilityAnimationTypeResetter {
 public:
  ScopedWindowVisibilityAnimationTypeResetter(aura::Window* window,
                                              int new_type)
      : window_(window) {
    DCHECK(window);

    if (::wm::GetWindowVisibilityAnimationType(window) == new_type) {
      // Clear so as not to do anything when we go out of scope.
      window_ = nullptr;
      return;
    }

    ::wm::SetWindowVisibilityAnimationType(window_, new_type);
  }

  ~ScopedWindowVisibilityAnimationTypeResetter() {
    if (window_) {
      ::wm::SetWindowVisibilityAnimationType(
          window_, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
    }
  }

  ScopedWindowVisibilityAnimationTypeResetter(
      const ScopedWindowVisibilityAnimationTypeResetter&) = delete;
  ScopedWindowVisibilityAnimationTypeResetter& operator=(
      const ScopedWindowVisibilityAnimationTypeResetter&) = delete;

 private:
  raw_ptr<aura::Window> window_;
};

// -----------------------------------------------------------------------------

bool InOverviewSession() {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  return overview_controller && overview_controller->InOverviewSession();
}

// Returns the bottom-most snapped window in the given |desk_container|, and
// nullptr if no such window was found.
aura::Window* GetBottomMostSnappedWindowForDeskContainer(
    aura::Window* desk_container) {
  DCHECK(desks_util::IsDeskContainer(desk_container));
  DCHECK(display::Screen::GetScreen()->InTabletMode());

  // For the active desk, only use the windows snapped in SplitViewController if
  // SplitView mode is active.
  SplitViewController* split_view_controller =
      SplitViewController::Get(desk_container);
  if (desks_util::IsActiveDeskContainer(desk_container) &&
      split_view_controller->InSplitViewMode()) {
    aura::Window* left_window = split_view_controller->primary_window();
    aura::Window* right_window = split_view_controller->secondary_window();
    for (aura::Window* child : desk_container->children()) {
      if (child == left_window || child == right_window)
        return child;
    }

    return nullptr;
  }

  // For the inactive desks, we can't use the SplitViewController, since it only
  // tracks left/right snapped windows in the active desk only.
  // TODO(afakhry|xdai): SplitViewController should be changed to track snapped
  // windows per desk per display.
  for (aura::Window* child : desk_container->children()) {
    if (WindowState::Get(child)->IsSnapped())
      return child;
  }

  return nullptr;
}

}  // namespace

// -----------------------------------------------------------------------------
// BackdropController::WindowAnimationWaiter:

// Observers an ongoing animation of |animating_window| and updates the backdrop
// once that animation completes.
class BackdropController::WindowAnimationWaiter
    : public ui::ImplicitAnimationObserver {
 public:
  WindowAnimationWaiter(BackdropController* owner,
                        aura::Window* animating_window)
      : owner_(owner), animating_window_(animating_window) {
    auto* animator = animating_window_->layer()->GetAnimator();
    DCHECK(animator->is_animating());
    const auto original_transition_duration = animator->GetTransitionDuration();
    // Don't let |settings| overwrite the existing animation's duration.
    ui::ScopedLayerAnimationSettings settings{animator};
    settings.SetTransitionDuration(original_transition_duration);
    settings.AddObserver(this);
  }

  ~WindowAnimationWaiter() override { StopObservingImplicitAnimations(); }

  WindowAnimationWaiter(const WindowAnimationWaiter&) = delete;
  WindowAnimationWaiter& operator=(const WindowAnimationWaiter&) = delete;

  aura::Window* animating_window() { return animating_window_; }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    // We keep this object alive until we update the backdrop, since we use the
    // member `owner_` below. This is not necessary, since we can cache it as
    // a local before we reset `window_animation_waiter_`, but this way things
    // are more deterministic, just in case.
    auto to_destroy = std::move(owner_->window_animation_waiter_);
    DCHECK_EQ(this, to_destroy.get());
    owner_->UpdateBackdrop();
  }

 private:
  raw_ptr<BackdropController> owner_;
  raw_ptr<aura::Window> animating_window_;
};

// -----------------------------------------------------------------------------

BackdropController::BackdropController(aura::Window* container)
    : root_window_(container->GetRootWindow()), container_(container) {
  DCHECK(container_);
  auto* shell = Shell::Get();
  SplitViewController::Get(root_window_)->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->accessibility_controller()->AddObserver(this);
  shell->wallpaper_controller()->AddObserver(this);
}

BackdropController::~BackdropController() {
  window_backdrop_observations_.RemoveAllObservations();
  auto* shell = Shell::Get();
  shell->accessibility_controller()->RemoveObserver(this);
  shell->wallpaper_controller()->RemoveObserver(this);
  if (shell->overview_controller())
    shell->overview_controller()->RemoveObserver(this);
  SplitViewController::Get(root_window_)->RemoveObserver(this);
  // TODO(oshima): animations won't work right with mus:
  // http://crbug.com/548396.
  Hide(/*destroy=*/true);
}

void BackdropController::OnWindowAddedToLayout(aura::Window* window) {
  if (DoesWindowCauseBackdropUpdates(window)) {
    window_backdrop_observations_.AddObservation(WindowBackdrop::Get(window));
    UpdateBackdrop();
  }
}

void BackdropController::OnWindowRemovedFromLayout(aura::Window* window) {
  WindowBackdrop* window_backdrop = WindowBackdrop::Get(window);
  if (window_backdrop_observations_.IsObservingSource(window_backdrop))
    window_backdrop_observations_.RemoveObservation(window_backdrop);

  if (DoesWindowCauseBackdropUpdates(window))
    UpdateBackdrop();
}

void BackdropController::OnChildWindowVisibilityChanged(aura::Window* window) {
  if (DoesWindowCauseBackdropUpdates(window))
    UpdateBackdrop();
}

void BackdropController::OnWindowStackingChanged(aura::Window* window) {
  if (DoesWindowCauseBackdropUpdates(window))
    UpdateBackdrop();
}

void BackdropController::OnPostWindowStateTypeChange(aura::Window* window) {
  // When `window` is snapped and about to be put into overview, the backdrop
  // can remain behind the window. We will hide the backdrop early to prevent it
  // from being seen during the overview starting animation.
  if (backdrop_ && backdrop_->IsVisible() &&
      WindowState::Get(window)->IsSnapped() &&
      SplitViewController::Get(window->GetRootWindow())
          ->WillStartPartialOverview(window)) {
    Hide(/*destroy=*/false, /*animate=*/false);
    return;
  }

  if (DoesWindowCauseBackdropUpdates(window))
    UpdateBackdrop();
}

void BackdropController::OnDisplayMetricsChanged() {
  // Display changes such as rotation, device scale factor, ... etc. don't
  // affect the visibility or availability of the backdrop. They may however
  // affect its bounds. So just layout.
  MaybeUpdateLayout();
}

void BackdropController::OnTabletModeChanged() {
  UpdateBackdrop();
}

void BackdropController::OnDeskContentChanged() {
  // This should *only* be called while overview is active. Otherwise, the
  // WorkspaceLayoutManager should take care of updating the backdrop.
  DCHECK(InOverviewSession());

  // Desk content changes may result in the need to update the backdrop when
  // overview is active, since the mini_view should show updated content.
  // Example: when the last window needing backdrop is moved to another desk,
  // the backdrop should be destroyed from the source desk, while created for
  // the target desk, and the mini_views of both desks should be updated.
  UpdateBackdropInternal();
}

void BackdropController::UpdateBackdrop() {
  // Skip updating while overview mode is active, since the backdrop is hidden.
  if (pause_update_ || InOverviewSession())
    return;

  UpdateBackdropInternal();
}

aura::Window* BackdropController::GetTopmostWindowWithBackdrop() {
  const aura::Window::Windows windows = container_->children();
  for (aura::Window* window : base::Reversed(windows)) {
    if (window == backdrop_window_)
      continue;

    if (window->GetType() != aura::client::WINDOW_TYPE_NORMAL)
      continue;

    auto* window_state = WindowState::Get(window);
    if (window_state->IsMinimized())
      continue;

    // No need to check the visibility or the activateability of the window if
    // this is not a desk container.
    if (desks_util::IsDeskContainer(container_)) {
      if (!window->layer()->GetTargetVisibility())
        continue;

      if (!wm::CanActivateWindow(window))
        continue;
    }

    if (!WindowShouldHaveBackdrop(window))
      continue;

    if (!window_util::ShouldShowForCurrentUser(window))
      continue;

    return window;
  }
  return nullptr;
}

void BackdropController::HideOnTakingInformedRestoreScreenshot() {
  Hide(/*destroy=*/false, /*animate=*/false);
}

base::ScopedClosureRunner BackdropController::PauseUpdates() {
  DCHECK(!pause_update_);

  pause_update_ = true;
  return base::ScopedClosureRunner(base::BindOnce(
      &BackdropController::RestoreUpdates, weak_ptr_factory_.GetWeakPtr()));
}

void BackdropController::OnOverviewModeStarting() {
  // Don't destroy backdrops, just hide them so they don't show in the overview
  // grid, but keep the widget so that it can be mirrored into the mini_desk
  // views.
  Hide(/*destroy=*/false, /*animate=*/false);
}

void BackdropController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  pause_update_ = true;
}

void BackdropController::OnOverviewModeEndingAnimationComplete(bool canceled) {
  pause_update_ = false;
  UpdateBackdrop();
}

void BackdropController::OnAccessibilityStatusChanged() {
  UpdateAccessibilityMode();
}

void BackdropController::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // Force the update of the backdrop, even if overview is active, so that the
  // backdrop shows up properly in the mini_views.
  UpdateBackdropInternal();
}

void BackdropController::OnSplitViewDividerPositionChanged() {
  MaybeUpdateLayout();
}

void BackdropController::OnWallpaperPreviewStarted() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (active_window) {
    WindowBackdrop::Get(active_window)
        ->SetBackdropMode(WindowBackdrop::BackdropMode::kDisabled);
  }
}

void BackdropController::OnWindowBackdropPropertyChanged(aura::Window* window) {
  if (DoesWindowCauseBackdropUpdates(window))
    UpdateBackdrop();
}

void BackdropController::RestoreUpdates() {
  pause_update_ = false;
  UpdateBackdrop();
}

void BackdropController::UpdateBackdropInternal() {
  // Skip the recursive updates.
  if (pause_update_)
    return;

  // Updating the back drop widget should not affect the shelf's auto hide
  // state.
  Shelf::ScopedAutoHideLock auto_hide_lock(ash::Shelf::ForWindow(container_));

  // We are either destroying the backdrop widget or changing the order of
  // windows which will cause recursion.
  base::AutoReset<bool> lock(&pause_update_, true);
  aura::Window* window = GetTopmostWindowWithBackdrop();

  if (window == window_having_backdrop_) {
    if (window)
      Show();
    return;
  }

  window_having_backdrop_ = window;

  if (!window_having_backdrop_) {
    // Destroy the backdrop since no suitable window was found.
    Hide(/*destroy=*/true);
    return;
  }

  DCHECK_EQ(window_having_backdrop_->GetRootWindow(), root_window_);
  DCHECK_NE(window_having_backdrop_, backdrop_window_);

  EnsureBackdropWidget();
  Show();
}

void BackdropController::EnsureBackdropWidget() {
  DCHECK(window_having_backdrop_);
  if (backdrop_)
    return;

  backdrop_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = container_->GetBoundsInScreen();
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.name = "Backdrop";
  // To disallow the MRU list from picking this window up it should not be
  // activateable.
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  DCHECK_NE(kShellWindowId_Invalid, container_->GetId());
  params.parent = container_;
  params.init_properties_container.SetProperty(kHideInOverviewKey, true);
  params.init_properties_container.SetProperty(kForceVisibleInMiniViewKey,
                                               true);
  backdrop_->Init(std::move(params));
  backdrop_window_ = backdrop_->GetNativeWindow();
  // The backdrop window in always on top container can be reparented without
  // this when the window is set to fullscreen.
  AlwaysOnTopController::SetDisallowReparent(backdrop_window_);
  backdrop_window_->layer()->SetColor(
      WindowBackdrop::Get(window_having_backdrop_)->GetBackdropColor());

  WindowState::Get(backdrop_window_)->set_allow_set_bounds_direct(true);
  UpdateAccessibilityMode();
}

void BackdropController::UpdateAccessibilityMode() {
  if (!backdrop_)
    return;

  const bool enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
  if (enabled) {
    if (!backdrop_event_handler_) {
      backdrop_event_handler_ = std::make_unique<BackdropEventHandler>();
      original_event_handler_ =
          backdrop_window_->SetTargetHandler(backdrop_event_handler_.get());
    }
  } else if (backdrop_event_handler_) {
    backdrop_window_->SetTargetHandler(original_event_handler_);
    backdrop_event_handler_.reset();
  }
}

bool BackdropController::WindowShouldHaveBackdrop(aura::Window* window) {
  WindowBackdrop* window_backdrop = WindowBackdrop::Get(window);
  if (window_backdrop->temporarily_disabled()) {
    return false;
  }

  WindowBackdrop::BackdropMode backdrop_mode = window_backdrop->mode();
  if (backdrop_mode == WindowBackdrop::BackdropMode::kEnabled) {
    return true;
  }
  if (backdrop_mode == WindowBackdrop::BackdropMode::kDisabled) {
    return false;
  }

  if (!desks_util::IsDeskContainer(container_)) {
    return false;
  }

  if (!display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }

  // Don't show the backdrop in tablet mode for PIP windows.
  auto* state = WindowState::Get(window);
  if (state->IsPip()) {
    return false;
  }

  if (!state->IsSnapped()) {
    return true;
  }

  auto* bottom_most_snapped_window =
      GetBottomMostSnappedWindowForDeskContainer(container_);
  if (!bottom_most_snapped_window) {
    return true;
  }
  return window == bottom_most_snapped_window;
}

void BackdropController::Show() {
  DCHECK(backdrop_);
  DCHECK(backdrop_window_);
  DCHECK(window_having_backdrop_);

  // No need to wait for window animations while in overview, since the backdrop
  // will be hidden anyways, but we still have to update its stacking and
  // layout.
  const bool in_overview = InOverviewSession();
  if (!in_overview && MaybeWaitForWindowAnimation())
    return;

  Layout();

  // Update backdrop color.
  const SkColor backdrop_color =
      WindowBackdrop::Get(window_having_backdrop_)->GetBackdropColor();
  if (backdrop_window_->layer()->GetTargetColor() != backdrop_color)
    backdrop_window_->layer()->SetColor(backdrop_color);

  // Update the stcking, only after we determine we can show the backdrop. The
  // backdrop needs to be immediately behind the window that needs a backdrop.
  container_->StackChildBelow(backdrop_window_, window_having_backdrop_);

  // When overview is active, the backdrop should never be shown. However, it
  // must be laid out, since it should show up properly in the mini_views.
  if (backdrop_->IsVisible() || in_overview)
    return;

  ScopedWindowVisibilityAnimationTypeResetter resetter{
      backdrop_window_,
      WindowState::Get(window_having_backdrop_)->CanMaximize()
          ? static_cast<int>(WINDOW_VISIBILITY_ANIMATION_TYPE_STEP_END)
          : static_cast<int>(::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE)};
  backdrop_->Show();
}

void BackdropController::Hide(bool destroy, bool animate) {
  if (!backdrop_ || is_hiding_backdrop_)
    return;

  DCHECK(backdrop_window_);
  base::AutoReset<bool> lock(&is_hiding_backdrop_, true);

  const aura::Window::Windows windows = container_->children();
  auto window_iter = base::ranges::find(windows, backdrop_window_);
  ++window_iter;
  if (window_iter != windows.end()) {
    aura::Window* window_above_backdrop = *window_iter;
    WindowState* window_state = WindowState::Get(window_above_backdrop);
    if (!animate || (window_state && window_state->CanMaximize()))
      backdrop_window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
  } else {
    // Window with backdrop may be destroyed before |backdrop_window_|. Hide the
    // backdrop window without animation in this case.
    backdrop_window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }

  if (destroy) {
    // The |backdrop_| widget owns the |backdrop_window_| so it will also be
    // deleted.
    backdrop_.reset();
    backdrop_window_ = nullptr;
    original_event_handler_ = nullptr;
    backdrop_event_handler_.reset();
  } else {
    backdrop_->Hide();
  }
}

bool BackdropController::BackdropShouldFullscreen() {
  // TODO(afakhry): Define the correct behavior and revise this in a follow-up
  // CL.
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  SplitViewController::State state = split_view_controller->state();
  if ((state == SplitViewController::State::kPrimarySnapped &&
       window_having_backdrop_ == split_view_controller->primary_window()) ||
      (state == SplitViewController::State::kSecondarySnapped &&
       window_having_backdrop_ == split_view_controller->secondary_window())) {
    return false;
  }

  return true;
}

gfx::Rect BackdropController::GetBackdropBounds() {
  DCHECK(!BackdropShouldFullscreen());

  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  SplitViewController::State state = split_view_controller->state();
  DCHECK(state == SplitViewController::State::kPrimarySnapped ||
         state == SplitViewController::State::kSecondarySnapped);
  SnapPosition snap_position =
      (state == SplitViewController::State::kPrimarySnapped)
          ? SnapPosition::kPrimary
          : SnapPosition::kSecondary;
  return split_view_controller->GetSnappedWindowBoundsInScreen(
      snap_position, /*window_for_minimum_size=*/nullptr,
      chromeos::kDefaultSnapRatio, /*account_for_divider_width=*/true);
}

void BackdropController::Layout() {
  DCHECK(backdrop_);

  // Makes sure that the backdrop has the correct bounds if it should not be
  // fullscreen size.
  backdrop_->SetFullscreen(BackdropShouldFullscreen());
  if (backdrop_->IsFullscreen()) {
    // TODO(oshima): The size of solid color layer can be smaller than texture's
    // layer with fractional scale (crbug.com/9000220). Use adjusted bounds so
    // that it can cover texture layer. Fix the bug and remove this.
    const gfx::Rect bounds =
        screen_util::GetDisplayBoundsInParent(backdrop_window_);
    backdrop_window_->SetBounds(
        screen_util::SnapBoundsToDisplayEdge(bounds, backdrop_window_));
  } else {
    backdrop_->SetBounds(GetBackdropBounds());
  }
}

bool BackdropController::MaybeWaitForWindowAnimation() {
  DCHECK(window_having_backdrop_);

  auto* animator = window_having_backdrop_->layer()->GetAnimator();
  if (!animator->is_animating())
    return false;

  if (window_animation_waiter_ &&
      window_animation_waiter_->animating_window() == window_having_backdrop_) {
    return true;
  }

  window_animation_waiter_.reset();

  constexpr int kCheckedAnimations = ui::LayerAnimationElement::BOUNDS |
                                     ui::LayerAnimationElement::TRANSFORM |
                                     ui::LayerAnimationElement::OPACITY |
                                     ui::LayerAnimationElement::VISIBILITY;
  if (!animator->IsAnimatingOnePropertyOf(kCheckedAnimations))
    return false;

  window_animation_waiter_ =
      std::make_unique<WindowAnimationWaiter>(this, window_having_backdrop_);
  return true;
}

void BackdropController::MaybeUpdateLayout() {
  if (backdrop_ && backdrop_->IsVisible())
    Layout();
}

bool BackdropController::DoesWindowCauseBackdropUpdates(
    aura::Window* window) const {
  // Popups should not result in any backdrop updates. We also avoid unnecessary
  // recursive calls to UpdateBackdrop() from the WorkspaceLayoutManager caused
  // by the backdrop itself, even though we avoid recursion here via
  // |pause_update_|.
  return window->GetType() != aura::client::WINDOW_TYPE_POPUP &&
         (!backdrop_ || window != backdrop_->GetNativeWindow());
}

}  // namespace ash
