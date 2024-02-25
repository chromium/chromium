// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/split_view_test_api.h"

#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"

namespace ash {

namespace {

SplitViewController* split_view_controller() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

}  // namespace

SplitViewTestApi::SplitViewTestApi() : controller_(split_view_controller()) {}

SplitViewTestApi::~SplitViewTestApi() = default;

void SplitViewTestApi::SnapWindow(aura::Window* window,
                                  SnapPosition snap_position) {
  controller_->SnapWindow(window, snap_position);
}

void SplitViewTestApi::SwapWindows() {
  controller_->SwapWindows();
}

aura::Window* SplitViewTestApi::GetPrimaryWindow() const {
  return controller_->primary_window();
}

aura::Window* SplitViewTestApi::GetSecondaryWindow() const {
  return controller_->secondary_window();
}

}  // namespace ash
