// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/backdrop_controller.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr SkColor kSemiOpaqueBackdropColor =
    SkColorSetARGB(0x99, 0x20, 0x21, 0x24);

SkColor GetBackdropColorByMode(BackdropWindowMode mode) {
  if (mode == BackdropWindowMode::kAutoSemiOpaque)
    return kSemiOpaqueBackdropColor;

  DCHECK(mode == BackdropWindowMode::kAutoOpaque ||
         mode == BackdropWindowMode::kEnabled);
  return SK_ColorBLACK;
}

class BackdropEventHandler : public ui::EventHandler {
 public:
  BackdropEventHandler() = default;
  ~BackdropEventHandler() override = default;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // If the event is targeted at the backdrop, it means the user has made an
    // interaction that is outside the window's bounds and we want to capture
    // it (usually when in spoken feedback mode). Handle the event (to prevent
    // behind-windows from receiving it) and play an earcon to notify the user.
    if (event->IsLocatedEvent()) {
      switch (event->type()) {
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSEWHEEL:
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_GESTURE_BEGIN:
        case ui::ET_SCROLL:
        case ui::ET_SCROLL_FLING_START:
          Shell::Get()->accessibility_controller()->PlayEarcon(
              chromeos::SOUND_VOLUME_ADJUST);
          break;
        default:
          break;
      }
      event->SetHandled();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackdropEventHandler);
};

bool InOverviewSession() {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  return overview_controller && overview_controller->InOverviewSession();
}

// Returns the bottom-most snapped window in the given |desk_container|, and
// nullptr if no such window was found.
aura::Window* GetBottomMostSnappedWindowForDeskContainer(
    aura::Window* desk_container) {
  DCHECK(desks_util::IsDeskContainer(desk_container));
  DCHECK(Shell::Get()->tablet_mode_controller()->InTabletMode());

  // For the active desk, only use the windows snapped in SplitViewController if
  // SplitView mode is active.
  SplitViewController* split_view_controller =
      SplitViewController::Get(desk_container);
  if (desks_util::IsActiveDeskContainer(desk_container) &&
      split_view_controller->InSplitViewMode()) {
    aura::Window* left_window = split_view_controller->left_window();
    aura::Window* right_window = split_view_controller->right_window();
    for (auto* child : desk_container->children()) {
      if (child == left_window || child == right_window)
        return child;
    }

    return nullptr;
  }

  // For the inactive desks, we can't use the SplitViewController, since it only
  // tracks left/right snapped windows in the active desk only.
  // TODO(afakhry|xdai): SplitViewController should be changed to track snapped
  // windows per desk per display.
  for (auto* child : desk_container->children()) {
    if (WindowState::Get(child)->IsSnapped())
      return child;
  }

  return nullptr;
}

}  // namespace

BackdropController::BackdropController(aura::Window* container)
    : root_window_(container->GetRootWindow()), container_(container) {
  DCHECK(container_);
  auto* shell = Shell::Get();
  SplitViewController::Get(root_window_)->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->accessibility_controller()->AddObserver(this);
  shell->wallpaper_controller()->AddObserver(this);
  shell->tablet_mode_controller()->AddObserver(this);
}

BackdropController::~BackdropController() {
  auto* shell = Shell::Get();
  // Shell destroys the TabletModeController before destroying all root windows.
  if (shell->tablet_mode_controller())
    shell->tablet_mode_controller()->RemoveObserver(this);
  shell->accessibility_controller()->RemoveObserver(this);
  shell->wallpaper_controller()->RemoveObserver(this);
  if (shell->overview_controller())
    shell->overview_controller()->RemoveObserver(this);
  SplitViewController::Get(root_window_)->RemoveObserver(this);
  // TODO(oshima): animations won't work right with mus:
  // http://crbug.com/548396.
  Hide(/*destroy=*/true);
}

void BackdropController::OnWindowAddedToLayout() {
  UpdateBackdrop();
}

void BackdropController::OnWindowRemovedFromLayout() {
  UpdateBackdrop();
}

void BackdropController::OnChildWindowVisibilityChanged() {
  UpdateBackdrop();
}

void BackdropController::OnWindowStackingChanged() {
  UpdateBackdrop();
}

void BackdropController::OnDisplayMetricsChanged() {
  UpdateBackdrop();
}

void BackdropController::OnPostWindowStateTypeChange() {
  UpdateBackdrop();
}

void BackdropController::OnDeskContentChanged() {
  // Desk content changes may result in the need to update the backdrop even
  // when overview is active, since the mini_view should show updated content.
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
  for (auto window_iter = windows.rbegin(); window_iter != windows.rend();
       ++window_iter) {
    aura::Window* window = *window_iter;
    if (window == backdrop_window_)
      continue;

    if (window->type() != aura::client::WINDOW_TYPE_NORMAL)
      continue;

    auto* window_state = WindowState::Get(window);
    if (window_state->IsMinimized())
      continue;

    // No need to check the visibility or the activateability of the window if
    // this is an inactive desk's container.
    if (!desks_util::IsDeskContainer(container_) ||
        desks_util::IsActiveDeskContainer(container_)) {
      if (!window->layer()->GetTargetVisibility())
        continue;

      if (!wm::CanActivateWindow(window))
        continue;
    }

    if (!WindowShouldHaveBackdrop(window))
      continue;

    return window;
  }
  return nullptr;
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
  UpdateBackdrop();
}

void BackdropController::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // Force the update of the backdrop, even if overview is active, so that the
  // backdrop shows up properly in the mini_views.
  UpdateBackdropInternal();
}

void BackdropController::OnSplitViewDividerPositionChanged() {
  UpdateBackdrop();
}

void BackdropController::OnWallpaperPreviewStarted() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (active_window) {
    active_window->SetProperty(kBackdropWindowMode,
                               BackdropWindowMode::kDisabled);
  }
  UpdateBackdrop();
}

void BackdropController::OnTabletModeStarted() {
  UpdateBackdrop();
}

void BackdropController::OnTabletModeEnded() {
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

  // We are either destroying the backdrop widget or changing the order of
  // windows which will cause recursion.
  base::AutoReset<bool> lock(&pause_update_, true);
  aura::Window* window = GetTopmostWindowWithBackdrop();
  if (!window) {
    // Destroy the backdrop since no suitable window was found.
    Hide(/*destroy=*/true);
    return;
  }

  EnsureBackdropWidget(window->GetProperty(kBackdropWindowMode));
  UpdateAccessibilityMode();

  if (window == backdrop_window_ && backdrop_->IsVisible()) {
    Layout();
    return;
  }
  if (window->GetRootWindow() != backdrop_window_->GetRootWindow())
    return;

  // Update the animation type of |backdrop_window_| based on current top most
  // window with backdrop.
  SetBackdropAnimationType(WindowState::Get(window)->CanMaximize()
                               ? WINDOW_VISIBILITY_ANIMATION_TYPE_STEP_END
                               : ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);

  Show();

  SetBackdropAnimationType(::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);

  // Backdrop needs to be immediately behind the window.
  container_->StackChildBelow(backdrop_window_, window);
}

void BackdropController::EnsureBackdropWidget(BackdropWindowMode mode) {
  if (backdrop_) {
    SkColor backdrop_color = GetBackdropColorByMode(mode);
    if (backdrop_window_->layer()->GetTargetColor() != backdrop_color)
      backdrop_window_->layer()->SetColor(backdrop_color);
    return;
  }

  backdrop_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = container_->GetBoundsInScreen();
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.name = "Backdrop";
  // To disallow the MRU list from picking this window up it should not be
  // activateable.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  DCHECK_NE(kShellWindowId_Invalid, container_->id());
  params.parent = container_;
  backdrop_->Init(std::move(params));
  backdrop_window_ = backdrop_->GetNativeWindow();
  backdrop_window_->SetProperty(kHideInOverviewKey, true);
  // The backdrop window in always on top container can be reparented without
  // this when the window is set to fullscreen.
  AlwaysOnTopController::SetDisallowReparent(backdrop_window_);
  backdrop_window_->layer()->SetColor(GetBackdropColorByMode(mode));

  WindowState::Get(backdrop_window_)->set_allow_set_bounds_direct(true);
}

void BackdropController::UpdateAccessibilityMode() {
  if (!backdrop_)
    return;

  bool enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback_enabled();
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
  if (window->GetAllPropertyKeys().count(kBackdropWindowMode)) {
    BackdropWindowMode backdrop_mode = window->GetProperty(kBackdropWindowMode);
    if (backdrop_mode == BackdropWindowMode::kEnabled)
      return true;
    if (backdrop_mode == BackdropWindowMode::kDisabled)
      return false;
  }

  // If |window| is the current active window and is an ARC app window, |window|
  // should have a backdrop when spoken feedback is enabled.
  if (window->GetProperty(aura::client::kAppType) ==
          static_cast<int>(AppType::ARC_APP) &&
      wm::IsActiveWindow(window) &&
      Shell::Get()->accessibility_controller()->spoken_feedback_enabled()) {
    return true;
  }

  if (!desks_util::IsDeskContainer(container_))
    return false;

  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    return false;

  // Don't show the backdrop in tablet mode for PIP windows.
  auto* state = WindowState::Get(window);
  if (state->IsPip())
    return false;

  if (!state->IsSnapped())
    return true;

  auto* bottom_most_snapped_window =
      GetBottomMostSnappedWindowForDeskContainer(container_);
  if (!bottom_most_snapped_window)
    return true;
  return window == bottom_most_snapped_window;
}

void BackdropController::Show() {
  Layout();

  // When overview is active, the backdrop should never be shown. However, it
  // must be laid out, since it should show up properly in the mini_views.
  if (!InOverviewSession())
    backdrop_->Show();
}

void BackdropController::Hide(bool destroy, bool animate) {
  if (!backdrop_)
    return;

  DCHECK(backdrop_window_);
  const aura::Window::Windows windows = container_->children();
  auto window_iter =
      std::find(windows.begin(), windows.end(), backdrop_window_);
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
  aura::Window* window = GetTopmostWindowWithBackdrop();
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  SplitViewController::State state = split_view_controller->state();
  if ((state == SplitViewController::State::kLeftSnapped &&
       window == split_view_controller->left_window()) ||
      (state == SplitViewController::State::kRightSnapped &&
       window == split_view_controller->right_window())) {
    return false;
  }

  return true;
}

gfx::Rect BackdropController::GetBackdropBounds() {
  DCHECK(!BackdropShouldFullscreen());

  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  SplitViewController::State state = split_view_controller->state();
  DCHECK(state == SplitViewController::State::kLeftSnapped ||
         state == SplitViewController::State::kRightSnapped);
  SplitViewController::SnapPosition snap_position =
      (state == SplitViewController::State::kLeftSnapped)
          ? SplitViewController::LEFT
          : SplitViewController::RIGHT;
  return split_view_controller->GetSnappedWindowBoundsInScreen(
      snap_position, /*window_for_minimum_size=*/nullptr);
}

void BackdropController::Layout() {
  // Makes sure that the backdrop has the correct bounds if it should not be
  // fullscreen size.
  backdrop_->SetFullscreen(BackdropShouldFullscreen());
  if (backdrop_->IsFullscreen()) {
    // TODO(oshima): The size of solid color layer can be smaller than texture's
    // layer with fractional scale (crbug.com/9000220). Use adjusted bounds so
    // that it can cover texture layer. Fix the bug and remove this.
    auto* window = backdrop_window_;
    gfx::Rect bounds = screen_util::GetDisplayBoundsInParent(window);
    backdrop_window_->SetBounds(
        screen_util::SnapBoundsToDisplayEdge(bounds, window));
  } else {
    backdrop_->SetBounds(GetBackdropBounds());
  }
}

void BackdropController::SetBackdropAnimationType(int type) {
  if (!backdrop_window_ ||
      ::wm::GetWindowVisibilityAnimationType(backdrop_window_) == type) {
    return;
  }

  ::wm::SetWindowVisibilityAnimationType(backdrop_window_, type);
}

}  // namespace ash
