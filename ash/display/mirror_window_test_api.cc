// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_test_api.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/root_window_transformer.h"
#include "ash/shell.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

std::vector<aura::WindowTreeHost*> MirrorWindowTestApi::GetHosts() const {
  std::vector<aura::WindowTreeHost*> hosts;
  for (aura::Window* window : Shell::Get()
                                  ->window_tree_host_manager()
                                  ->mirror_window_controller()
                                  ->GetAllRootWindows()) {
    hosts.emplace_back(window->GetHost());
  }
  return hosts;
}

ui::mojom::CursorType MirrorWindowTestApi::GetCurrentCursorType() const {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->cursor_.type();
}

const gfx::Point& MirrorWindowTestApi::GetCursorHotPoint() const {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->hot_point_;
}

gfx::Point MirrorWindowTestApi::GetCursorHotPointLocationInRootWindow() const {
  return Shell::Get()
             ->window_tree_host_manager()
             ->cursor_window_controller()
             ->GetCursorBoundsInScreenForTest()
             .origin() +
         GetCursorHotPoint().OffsetFromOrigin();
}

const aura::Window* MirrorWindowTestApi::GetCursorHostWindow() const {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->GetCursorHostWindowForTest();
}

gfx::Point MirrorWindowTestApi::GetCursorLocation() const {
  gfx::Point point = Shell::Get()
                         ->window_tree_host_manager()
                         ->cursor_window_controller()
                         ->GetCursorBoundsInScreenForTest()
                         .origin();
  const gfx::Point hot_point = GetCursorHotPoint();
  point.Offset(hot_point.x(), hot_point.y());
  return point;
}

}  // namespace ash
