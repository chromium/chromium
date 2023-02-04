// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_float_container_stacker.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

namespace ash {

ScopedFloatContainerStacker::ScopedFloatContainerStacker(
    OverviewWindowDragController* owner)
    : owner_(owner) {
  // Dragging can happen across multiple displays. Place the float container
  // under the desk containers while this object lives.
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    aura::Window* desk_container =
        root->GetChildById(kShellWindowId_DeskContainerA);
    aura::Window* float_container =
        root->GetChildById(kShellWindowId_FloatContainer);
    float_container->parent()->StackChildBelow(float_container, desk_container);
  }
}

ScopedFloatContainerStacker::~ScopedFloatContainerStacker() {
  if (animation_observer_) {
    DCHECK(dragged_window_);
    dragged_window_->layer()->GetAnimator()->RemoveObserver(
        animation_observer_.get());
  }

  // Restack the float container below the app list container.
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    aura::Window* app_list_container =
        root->GetChildById(kShellWindowId_AppListContainer);
    aura::Window* float_container =
        root->GetChildById(kShellWindowId_FloatContainer);
    float_container->parent()->StackChildBelow(float_container,
                                               app_list_container);
  }
}

void ScopedFloatContainerStacker::Shutdown(aura::Window* dragged_window) {
  auto* animator = dragged_window->layer()->GetAnimator();
  if (!animator->is_animating()) {
    // Destroys `this`.
    owner_->DestroyFloatDragHelper();
    return;
  }

  dragged_window_ = dragged_window;
  dragged_window_observation_.Observe(dragged_window);
  animation_observer_ = std::make_unique<ui::CallbackLayerAnimationObserver>(
      base::BindRepeating(&ScopedFloatContainerStacker::OnAnimationsCompleted,
                          base::Unretained(this)));
  animator->AddObserver(animation_observer_.get());
  animation_observer_->SetActive();
}

void ScopedFloatContainerStacker::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(dragged_window_, window);

  if (animation_observer_) {
    dragged_window_->layer()->GetAnimator()->RemoveObserver(
        animation_observer_.get());
  }
  animation_observer_.reset();
  dragged_window_ = nullptr;
  dragged_window_observation_.Reset();

  // Destroys `this`.
  owner_->DestroyFloatDragHelper();
}

bool ScopedFloatContainerStacker::OnAnimationsCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // Destroys `this`.
  owner_->DestroyFloatDragHelper();
  // Returns false so the observer does not self delete. `this` will control the
  // lifetime of the observer.
  return false;
}

}  // namespace ash
