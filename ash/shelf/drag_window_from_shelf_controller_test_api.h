// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_TEST_API_H_
#define ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"

namespace ui {
class Layer;
}

namespace ash {

class DragWindowFromShelfController;

class ASH_EXPORT DragWindowFromShelfControllerTestApi {
 public:
  DragWindowFromShelfControllerTestApi();
  DragWindowFromShelfControllerTestApi(
      const DragWindowFromShelfControllerTestApi&) = delete;
  DragWindowFromShelfControllerTestApi& operator=(
      const DragWindowFromShelfControllerTestApi&) = delete;
  ~DragWindowFromShelfControllerTestApi();

  void WaitUntilOverviewIsShown(
      DragWindowFromShelfController* window_drag_controller);

  // Retrieves the copy layer of the "other" window during a drag from shelf
  // with a floated window. See `DragWindowFromShelfController::other_window_`
  // for more details.
  ui::Layer* GetOtherWindowCopyLayer(
      DragWindowFromShelfController* window_drag_controller);
};

}  // namespace ash

#endif  // ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_TEST_API_H_
