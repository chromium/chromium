// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CONTAINER_FINDER_H_
#define ASH_WM_CONTAINER_FINDER_H_

#include <vector>

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {

// Returns the first ancestor of |window| that has a known container ID.
ASH_EXPORT aura::Window* GetContainerForWindow(aura::Window* window);

// Returns the parent to add |window| to. This is generally used when a window
// is moved from one root to another.
ASH_EXPORT aura::Window* GetDefaultParentForWindow(
    aura::Window* window,
    const gfx::Rect& bounds_in_screen);

// Returns the list of containers that match |container_id| in all root windows.
// If |priority_root| is non-null, the container in |priority_root| is placed at
// the front of the list.
ASH_EXPORT std::vector<aura::Window*> GetContainersForAllRootWindows(
    int container_id,
    aura::Window* priority_root = nullptr);

}  // namespace ash

#endif  // ASH_WM_CONTAINER_FINDER_H_
