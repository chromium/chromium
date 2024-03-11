// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_float_container_stacker.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

namespace ash {

namespace {

void StackFloatContainerAbove() {
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    aura::Window* always_on_top_container =
        root->GetChildById(kShellWindowId_AlwaysOnTopContainer);
    aura::Window* float_container =
        root->GetChildById(kShellWindowId_FloatContainer);
    float_container->parent()->StackChildAbove(float_container,
                                               always_on_top_container);
  }
}

void StackFloatContainerBelow() {
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    aura::Window* desk_container =
        root->GetChildById(kShellWindowId_DeskContainerA);
    aura::Window* float_container =
        root->GetChildById(kShellWindowId_FloatContainer);
    float_container->parent()->StackChildBelow(float_container, desk_container);
  }
}

}  // namespace

ScopedFloatContainerStacker::ScopedFloatContainerStacker() {
  StackFloatContainerBelow();
}

ScopedFloatContainerStacker::~ScopedFloatContainerStacker() {
  is_destroying_ = true;

  Cleanup();

  // Restack the float container below the app list container.
  StackFloatContainerAbove();
}

void ScopedFloatContainerStacker::OnDragStarted(aura::Window* dragged_window) {
  DCHECK(dragged_window);

  if (dragged_window != window_util::GetFloatedWindowForActiveDesk()) {
    return;
  }

  Cleanup();
  StackFloatContainerAbove();
}

void ScopedFloatContainerStacker::OnDragFinished(aura::Window* dragged_window) {
  auto* animator =
      dragged_window ? dragged_window->layer()->GetAnimator() : nullptr;
  if (!animator || !animator->is_animating() ||
      dragged_window != window_util::GetFloatedWindowForActiveDesk()) {
    StackFloatContainerBelow();
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
  Cleanup();
  StackFloatContainerBelow();
}

bool ScopedFloatContainerStacker::OnAnimationsCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  if (is_destroying_) {
    return false;
  }

  Cleanup();
  StackFloatContainerBelow();

  // Returns false so the observer does not self delete. `this` will control the
  // lifetime of the observer.
  return false;
}

void ScopedFloatContainerStacker::Cleanup() {
  if (animation_observer_) {
    dragged_window_->layer()->GetAnimator()->RemoveObserver(
        animation_observer_.get());
  }
  animation_observer_.reset();
  dragged_window_ = nullptr;
  dragged_window_observation_.Reset();
}

}  // namespace ash
