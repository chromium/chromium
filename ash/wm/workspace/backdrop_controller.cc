// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/backdrop_controller.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_delegate.h"
#include "base/auto_reset.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

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

}  // namespace

BackdropController::BackdropController(aura::Window* container)
    : container_(container), in_restacking_(false) {
  DCHECK(container_);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->accessibility_controller()->AddObserver(this);
  Shell::Get()->wallpaper_controller()->AddObserver(this);
}

BackdropController::~BackdropController() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  Shell::Get()->wallpaper_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  // TODO(oshima): animations won't work right with mus:
  // http://crbug.com/548396.
  Hide();
}

void BackdropController::OnWindowAddedToLayout(aura::Window* child) {
  UpdateBackdrop();
}

void BackdropController::OnWindowRemovedFromLayout(aura::Window* child) {
  UpdateBackdrop();
}

void BackdropController::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {
  UpdateBackdrop();
}

void BackdropController::OnWindowStackingChanged(aura::Window* window) {
  UpdateBackdrop();
}

void BackdropController::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    mojom::WindowStateType old_type) {
  UpdateBackdrop();
}

void BackdropController::SetBackdropDelegate(
    std::unique_ptr<BackdropDelegate> delegate) {
  delegate_ = std::move(delegate);
  UpdateBackdrop();
}

void BackdropController::UpdateBackdrop() {
  // No need to continue update for recursive calls or in overview mode.
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  if (in_restacking_ || (window_selector_controller &&
                         window_selector_controller->IsSelecting())) {
    return;
  }

  aura::Window* window = GetTopmostWindowWithBackdrop();
  if (!window) {
    // Hide backdrop since no suitable window was found.
    Hide();
    return;
  }
  // We are changing the order of windows which will cause recursion.
  base::AutoReset<bool> lock(&in_restacking_, true);
  EnsureBackdropWidget();
  UpdateAccessibilityMode();

  if (window == backdrop_window_ && backdrop_->IsVisible())
    return;
  if (window->GetRootWindow() != backdrop_window_->GetRootWindow())
    return;

  Show();

  // Since the backdrop needs to be immediately behind the window and the
  // stacking functions only guarantee a "it's above or below", we need
  // to re-arrange the two windows twice.
  container_->StackChildAbove(backdrop_window_, window);
  container_->StackChildAbove(window, backdrop_window_);
}

void BackdropController::OnOverviewModeStarting() {
  Hide();
}

void BackdropController::OnOverviewModeEnded() {
  UpdateBackdrop();
}

void BackdropController::OnAppListVisibilityChanged(bool shown,
                                                    aura::Window* root_window) {
  UpdateBackdrop();
}

void BackdropController::OnSplitViewModeStarting() {
  Shell::Get()->split_view_controller()->AddObserver(this);
}

void BackdropController::OnSplitViewModeEnded() {
  Shell::Get()->split_view_controller()->RemoveObserver(this);
}

void BackdropController::OnAccessibilityStatusChanged() {
  UpdateBackdrop();
}

void BackdropController::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  UpdateBackdrop();
}

void BackdropController::OnSplitViewDividerPositionChanged() {
  UpdateBackdrop();
}

void BackdropController::OnWallpaperPreviewStarted() {
  wm::GetActiveWindow()->SetProperty(kBackdropWindowMode,
                                     BackdropWindowMode::kDisabled);
  UpdateBackdrop();
}

void BackdropController::EnsureBackdropWidget() {
  if (backdrop_)
    return;

  backdrop_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = container_->GetBoundsInScreen();
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.name = "WorkspaceBackdropDelegate";
  // To disallow the MRU list from picking this window up it should not be
  // activateable.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  DCHECK_NE(kShellWindowId_Invalid, container_->id());
  params.parent = container_;
  backdrop_->Init(params);
  backdrop_window_ = backdrop_->GetNativeWindow();
  backdrop_window_->SetName("Backdrop");
  ::wm::SetWindowVisibilityAnimationType(
      backdrop_window_, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  backdrop_window_->layer()->SetColor(SK_ColorBLACK);
}

void BackdropController::UpdateAccessibilityMode() {
  if (!backdrop_)
    return;

  bool enabled =
      Shell::Get()->accessibility_controller()->IsSpokenFeedbackEnabled();
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

aura::Window* BackdropController::GetTopmostWindowWithBackdrop() {
  const aura::Window::Windows windows = container_->children();
  for (auto window_iter = windows.rbegin(); window_iter != windows.rend();
       ++window_iter) {
    aura::Window* window = *window_iter;
    if (window != backdrop_window_ && window->layer()->GetTargetVisibility() &&
        window->type() == aura::client::WINDOW_TYPE_NORMAL &&
        ::wm::CanActivateWindow(window) && WindowShouldHaveBackdrop(window)) {
      return window;
    }
  }
  return nullptr;
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
      Shell::Get()->accessibility_controller()->IsSpokenFeedbackEnabled()) {
    return true;
  }

  return delegate_ ? delegate_->HasBackdrop(window) : false;
}

void BackdropController::Show() {
  // Makes sure that the backdrop has the correct bounds if it should not be
  // fullscreen size.
  backdrop_->SetFullscreen(BackdropShouldFullscreen());
  if (!BackdropShouldFullscreen())
    backdrop_->SetBounds(GetBackdropBounds());
  backdrop_->Show();
}

void BackdropController::Hide() {
  if (!backdrop_)
    return;
  backdrop_->Close();
  backdrop_ = nullptr;
  backdrop_window_ = nullptr;
  original_event_handler_ = nullptr;
  backdrop_event_handler_.reset();
}

bool BackdropController::BackdropShouldFullscreen() {
  aura::Window* window = GetTopmostWindowWithBackdrop();
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  SplitViewController::State state = split_view_controller->state();
  if ((state == SplitViewController::LEFT_SNAPPED &&
       window == split_view_controller->left_window()) ||
      (state == SplitViewController::RIGHT_SNAPPED &&
       window == split_view_controller->right_window())) {
    return false;
  }

  return true;
}

gfx::Rect BackdropController::GetBackdropBounds() {
  DCHECK(!BackdropShouldFullscreen());

  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  SplitViewController::State state = split_view_controller->state();
  DCHECK(state == SplitViewController::LEFT_SNAPPED ||
         state == SplitViewController::RIGHT_SNAPPED);
  aura::Window* snapped_window =
      split_view_controller->GetDefaultSnappedWindow();
  SplitViewController::SnapPosition snap_position =
      (state == SplitViewController::LEFT_SNAPPED) ? SplitViewController::LEFT
                                                   : SplitViewController::RIGHT;
  return split_view_controller->GetSnappedWindowBoundsInScreenUnadjusted(
      snapped_window, snap_position);
}

}  // namespace ash
