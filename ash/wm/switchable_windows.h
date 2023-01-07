// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SWITCHABLE_WINDOWS_H_
#define ASH_WM_SWITCHABLE_WINDOWS_H_

#include <vector>

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

// If |active_desk_only| is true, non-active desks' containers will be excluded.
ASH_EXPORT std::vector<aura::Window*> GetSwitchableContainersForRoot(
    aura::Window* root,
    bool active_desk_only);

// Returns true if |window| is a container for windows which can be switched to.
ASH_EXPORT bool IsSwitchableContainer(const aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_SWITCHABLE_WINDOWS_H_
