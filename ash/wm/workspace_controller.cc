// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_animations.h"

// Defines a window property to store a WorkspaceController in the properties of
// virtual desks container windows.
ASH_EXPORT extern const aura::WindowProperty<ash::WorkspaceController*>* const
    kWorkspaceController;

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::WorkspaceController*)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ash::WorkspaceController,
                                   kWorkspaceController,
                                   nullptr)

namespace ash {
namespace {

// Amount of time to pause before animating anything. Only used during initial
// animation (when logging in).
const int kInitialPauseTimeMS = 750;

// The duration of the animation that occurs on first login.
const int kInitialAnimationDurationMS = 200;

}  // namespace

WorkspaceController::WorkspaceController(aura::Window* viewport)
    : viewport_(viewport),
      event_handler_(std::make_unique<WorkspaceEventHandler>(viewport)) {
  viewport_->SetLayoutManager(
      std::make_unique<WorkspaceLayoutManager>(viewport));
  viewport_->AddObserver(this);
  ::wm::SetWindowVisibilityAnimationTransition(viewport_, ::wm::ANIMATE_NONE);
}

WorkspaceController::~WorkspaceController() {
  if (!viewport_)
    return;

  viewport_->RemoveObserver(this);
  viewport_->SetLayoutManager(nullptr);
}

WorkspaceLayoutManager* WorkspaceController::layout_manager() {
  return static_cast<WorkspaceLayoutManager*>(viewport_->layout_manager());
}

WorkspaceWindowState WorkspaceController::GetWindowState() const {
  if (!viewport_)
    return WorkspaceWindowState::kDefault;

  // Always use DEFAULT state in overview mode so that work area stays
  // the same regardles of the window we have.
  // The |overview_controller| can be null during shutdown.
  if (Shell::Get()->overview_controller() &&
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return WorkspaceWindowState::kDefault;
  }

  const aura::Window* fullscreen =
      GetWindowForFullscreenModeForContext(viewport_);
  if (fullscreen)
    return WorkspaceWindowState::kFullscreen;

  auto mru_list =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);

  for (aura::Window* window : mru_list) {
    if (window->GetRootWindow() != viewport_->GetRootWindow())
      continue;
    WindowState* window_state = WindowState::Get(window);
    if (window->layer() && !window->layer()->GetTargetVisibility())
      continue;
    if (window_state->IsMaximized())
      return WorkspaceWindowState::kMaximized;
  }
  return WorkspaceWindowState::kDefault;
}

void WorkspaceController::DoInitialAnimation() {
  viewport_->Show();

  ui::Layer* layer = viewport_->layer();
  layer->SetOpacity(0.0f);
  SetTransformForScaleAnimation(layer, LAYER_SCALE_ANIMATION_ABOVE);

  // In order for pause to work we need to stop animations.
  layer->GetAnimator()->StopAnimating();

  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());

    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    layer->GetAnimator()->SchedulePauseForProperties(
        base::Milliseconds(kInitialPauseTimeMS),
        ui::LayerAnimationElement::TRANSFORM |
            ui::LayerAnimationElement::OPACITY |
            ui::LayerAnimationElement::BRIGHTNESS |
            ui::LayerAnimationElement::VISIBILITY);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    settings.SetTransitionDuration(
        base::Milliseconds(kInitialAnimationDurationMS));
    layer->SetTransform(gfx::Transform());
    layer->SetOpacity(1.0f);
  }
}

void WorkspaceController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, viewport_);
  viewport_->RemoveObserver(this);
  viewport_ = nullptr;
  // Destroy |event_handler_| too as it depends upon |window|.
  event_handler_.reset();
}

void SetWorkspaceController(
    aura::Window* desk_container,
    std::unique_ptr<WorkspaceController> workspace_controller) {
  DCHECK(desk_container);
  DCHECK(desks_util::IsDeskContainer(desk_container));

  if (workspace_controller)
    desk_container->SetProperty(kWorkspaceController,
                                std::move(workspace_controller));
  else
    desk_container->ClearProperty(kWorkspaceController);
}

WorkspaceController* GetWorkspaceController(aura::Window* desk_container) {
  DCHECK(desk_container);
  DCHECK(desks_util::IsDeskContainer(desk_container));

  return desk_container->GetProperty(kWorkspaceController);
}

WorkspaceController* GetWorkspaceControllerForContext(aura::Window* context) {
  DCHECK(!context->IsRootWindow());

  // Find the desk container to which |context| belongs.
  while (context && !desks_util::IsDeskContainer(context))
    context = context->parent();

  if (!context)
    return nullptr;

  return GetWorkspaceController(context);
}

WorkspaceController* GetActiveWorkspaceController(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  return GetWorkspaceController(
      desks_util::GetActiveDeskContainerForRoot(root));
}

}  // namespace ash
