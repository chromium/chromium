// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/window_util.h"

#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/window.h"

namespace crosapi {
namespace {

// Performs a depth-first search for a window with a given exo ShellSurface
// |app_id| starting at |root|.
aura::Window* FindWindowWithShellAppId(aura::Window* root,
                                       const std::string& app_id) {
  const std::string* id = exo::GetShellApplicationId(root);
  if (id && *id == app_id) {
    // Do not include a window still being created.
    auto* window_state = ash::WindowState::Get(root);
    if (!root->IsVisible() && !window_state->IsMinimized())
      return nullptr;
    return root;
  }
  for (aura::Window* child : root->children()) {
    aura::Window* found = FindWindowWithShellAppId(child, app_id);
    if (found)
      return found;
  }
  return nullptr;
}

}  // namespace

aura::Window* GetShellSurfaceWindow(const std::string& app_id) {
  for (aura::Window* display_root : ash::Shell::GetAllRootWindows()) {
    aura::Window* window = FindWindowWithShellAppId(display_root, app_id);
    if (window)
      return window;
  }
  return nullptr;
}

}  // namespace crosapi
