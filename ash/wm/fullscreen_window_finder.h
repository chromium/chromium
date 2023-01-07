// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FULLSCREEN_WINDOW_FINDER_H_
#define ASH_WM_FULLSCREEN_WINDOW_FINDER_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

// Returns the topmost window or one of its transient parents, if any of them
// are in fullscreen mode. This searches for a window in the switchable
// container parent of |context|. This can be used to find if there's a
// fullscreen window in the desk container of |context| or the always on top
// container if |context| belongs to it.
ASH_EXPORT aura::Window* GetWindowForFullscreenModeForContext(
    aura::Window* context);

// Returns the topmost window or one of its transient parents, if any of them
// are in fullscreen mode. This searches for a window in |root|. This considers
// only the always-on-top container or the active desk container.
ASH_EXPORT aura::Window* GetWindowForFullscreenModeInRoot(aura::Window* root);

}  // namespace ash

#endif  // ASH_WM_FULLSCREEN_WINDOW_FINDER_H_
