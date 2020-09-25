// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"

#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"

namespace ash {

RootWindowDeskSwitchAnimatorTestApi::RootWindowDeskSwitchAnimatorTestApi(
    RootWindowDeskSwitchAnimator* animator)
    : animator_(animator) {
  DCHECK(animator_);
}

RootWindowDeskSwitchAnimatorTestApi::~RootWindowDeskSwitchAnimatorTestApi() =
    default;

ui::Layer* RootWindowDeskSwitchAnimatorTestApi::GetAnimationLayer() {
  return animator_->animation_layer_owner_->root();
}

ui::Layer*
RootWindowDeskSwitchAnimatorTestApi::GetScreenshotLayerOfDeskWithIndex(
    int desk_index) {
  auto screenshot_layers = animator_->screenshot_layers_;

  DCHECK_GE(desk_index, 0);
  DCHECK_LT(desk_index, int{screenshot_layers.size()});

  ui::Layer* layer = screenshot_layers[desk_index];
  DCHECK(layer);
  return layer;
}

int RootWindowDeskSwitchAnimatorTestApi::GetEndingDeskIndex() const {
  return animator_->ending_desk_index_;
}

}  // namespace ash
