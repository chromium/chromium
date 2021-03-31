// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_TEST_API_H_
#define ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/shelf/drag_window_from_shelf_controller.h"
#include "base/run_loop.h"

namespace ash {

class ASH_EXPORT DragWindowFromShelfControllerTestApi
    : public DragWindowFromShelfController::Observer {
 public:
  DragWindowFromShelfControllerTestApi();
  ~DragWindowFromShelfControllerTestApi() override;

  void WaitUntilOverviewIsShown(
      DragWindowFromShelfController* window_drag_controller);

  // DragWindowFromShelfController::Observer:
  void OnOverviewVisibilityChanged(bool visible) override;

 private:
  std::unique_ptr<base::RunLoop> show_overview_waiter_;

  DragWindowFromShelfControllerTestApi(
      const DragWindowFromShelfControllerTestApi&) = delete;
  DragWindowFromShelfControllerTestApi& operator=(
      const DragWindowFromShelfControllerTestApi&) = delete;
};

}  // namespace ash

#endif  // ASH_SHELF_DRAG_WINDOW_FROM_SHELF_CONTROLLER_TEST_API_H_
