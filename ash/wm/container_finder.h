// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CONTAINER_FINDER_H_
#define ASH_WM_CONTAINER_FINDER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {

// Returns the first ancestor of |window| that has a known container ID.
ASH_EXPORT aura::Window* GetContainerForWindow(aura::Window* window);

// Returns the parent to add |window| to. This is used to find a proper parent
// for new widget, or a new window. The parent will be picked from a window tree
// in 'root_window' but if there is a better root window candidate that matches
// 'bounds_in_screen', then it will be used instead.
ASH_EXPORT aura::Window* GetDefaultParentForWindow(
    aura::Window* window,
    aura::Window* root_window,
    const gfx::Rect& bounds_in_screen);

// Returns the list of containers that match |container_id| in all root windows.
// If |priority_root| is non-null, the container in |priority_root| is placed at
// the front of the list.
ASH_EXPORT std::vector<raw_ptr<aura::Window, VectorExperimental>>
GetContainersForAllRootWindows(int container_id,
                               aura::Window* priority_root = nullptr);

// Returns the parent window for power button menu container for the provided
// root_Window.
ASH_EXPORT aura::Window* GetPowerMenuContainerParent(aura::Window* root_window);

}  // namespace ash

#endif  // ASH_WM_CONTAINER_FINDER_H_
