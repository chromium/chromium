// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_tree_host_lookup.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"

namespace ash {

aura::WindowTreeHost* GetWindowTreeHostForDisplay(int64_t display_id) {
  auto* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  return root_window_controller ? root_window_controller->GetHost() : nullptr;
}

}  // namespace ash
