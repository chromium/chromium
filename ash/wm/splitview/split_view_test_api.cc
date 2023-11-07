// Copyright 2019 The Chromium Authors
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
      position = SplitViewController::SnapPosition::kNone;
      break;
    case SnapPosition::LEFT:
      position = SplitViewController::SnapPosition::kPrimary;
      break;
    case SnapPosition::RIGHT:
      position = SplitViewController::SnapPosition::kSecondary;
      break;
  }
  split_view_controller()->SnapWindow(window, position);
}

void SplitViewTestApi::SwapWindows() {
  split_view_controller()->SwapWindows();
}

aura::Window* SplitViewTestApi::GetLeftWindow() const {
  return split_view_controller()->primary_window();
}

aura::Window* SplitViewTestApi::GetRightWindow() const {
  return split_view_controller()->secondary_window();
}

}  // namespace ash
