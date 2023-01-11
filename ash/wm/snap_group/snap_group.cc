// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include "ash/shell.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

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
  // two windows are always placed on top.
  aura::Window* target_window = gained_active == window1_ ? window2_ : window1_;
  target_window->parent()->StackChildBelow(target_window, gained_active);
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