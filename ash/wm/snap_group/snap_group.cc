// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// TODO(michelefan@): Remove this logic when implementing split view divider for
// arm2. Divider should be available for both arm1 and arm2.
bool ShouldConsiderDivider() {
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  return snap_group_controller &&
         snap_group_controller->IsArm1AutomaticallyLockEnabled();
}
}  // namespace

SnapGroup::SnapGroup(aura::Window* window1, aura::Window* window2) {
  StartObservingWindows(window1, window2);
}

SnapGroup::~SnapGroup() {
  StopObservingWindows();
}

void SnapGroup::OnWindowDestroying(aura::Window* window) {
  if (window != window1_ && window != window2_) {
    return;
  }

  // `this` will be destroyed after this line.
  Shell::Get()->snap_group_controller()->RemoveSnapGroup(this);
}

void SnapGroup::OnWindowActivated(ActivationReason reason,
                                  aura::Window* gained_active,
                                  aura::Window* lost_active) {
  if (gained_active != window1_ && gained_active != window2_) {
    return;
  }

  // Update the stacking order of the other window in the snap group so that the
  // two windows are always placed on top, both of which will be stacked below
  // the `split_view_divider` if applicable afterwards.
  aura::Window* target_window = gained_active == window1_ ? window2_ : window1_;
  auto* parent_container = target_window->parent();
  parent_container->StackChildBelow(target_window, gained_active);

  // Update the stacking order of the `split_view_divider` to be on top of the
  // `gained_active` window which makes the overall stacking order become
  // `divider_widget->GetNativeWindow()` --> `gained_active` -->
  // `target_window`.
  if (ShouldConsiderDivider()) {
    aura::Window* root_window = window1_->GetRootWindow();
    auto* split_view_controller = SplitViewController::Get(root_window);
    DCHECK(split_view_controller);
    auto* split_view_divider = split_view_controller->split_view_divider();
    DCHECK(split_view_divider);
    auto* divider_widget = split_view_divider->divider_widget();
    DCHECK(divider_widget);
    parent_container->StackChildAbove(divider_widget->GetNativeWindow(),
                                      gained_active);
  }
}

void SnapGroup::StartObservingWindows(aura::Window* window1,
                                      aura::Window* window2) {
  DCHECK(window1);
  DCHECK(window2);
  window1_ = window1;
  window1_->AddObserver(this);
  window2_ = window2;
  window2_->AddObserver(this);

  Shell::Get()->activation_client()->AddObserver(this);
}

void SnapGroup::StopObservingWindows() {
  if (window1_) {
    window1_->RemoveObserver(this);
    window1_ = nullptr;
  }

  if (window2_) {
    window2_->RemoveObserver(this);
    window2_ = nullptr;
  }

  Shell::Get()->activation_client()->RemoveObserver(this);
}

}  // namespace ash