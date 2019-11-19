// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/split_view_test_api.h"

#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"

namespace ash {

namespace {

SplitViewController* split_view_controller() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

}  // namespace

SplitViewTestApi::SplitViewTestApi() = default;

SplitViewTestApi::~SplitViewTestApi() = default;

void SplitViewTestApi::SnapWindow(
    aura::Window* window,
    SplitViewTestApi::SnapPosition snap_position) {
  SplitViewController::SnapPosition position;
  switch (snap_position) {
    case SnapPosition::NONE:
      position = SplitViewController::NONE;
      break;
    case SnapPosition::LEFT:
      position = SplitViewController::LEFT;
      break;
    case SnapPosition::RIGHT:
      position = SplitViewController::RIGHT;
      break;
  }
  split_view_controller()->SnapWindow(window, position);
}

void SplitViewTestApi::SwapWindows() {
  split_view_controller()->SwapWindows();
}

}  // namespace ash
