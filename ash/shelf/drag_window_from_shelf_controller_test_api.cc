// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/drag_window_from_shelf_controller_test_api.h"

#include "ash/shelf/drag_window_from_shelf_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/run_loop.h"

namespace ash {

DragWindowFromShelfControllerTestApi::DragWindowFromShelfControllerTestApi() =
    default;

DragWindowFromShelfControllerTestApi::~DragWindowFromShelfControllerTestApi() =
    default;

void DragWindowFromShelfControllerTestApi::WaitUntilOverviewIsShown(
    DragWindowFromShelfController* window_drag_controller) {
  DCHECK(window_drag_controller);

  if (!Shell::Get()->overview_controller()->InOverviewSession() ||
      window_drag_controller->show_overview_windows_) {
    return;
  }

  base::RunLoop run_loop;
  window_drag_controller->on_overview_shown_callback_for_testing_ =
      run_loop.QuitClosure();
  run_loop.Run();
}

ui::Layer* DragWindowFromShelfControllerTestApi::GetOtherWindowCopyLayer(
    DragWindowFromShelfController* window_drag_controller) {
  ui::LayerTreeOwner* layer_tree_owner =
      window_drag_controller->other_window_copy_.get();
  return layer_tree_owner ? layer_tree_owner->root() : nullptr;
}

}  // namespace ash
