// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_FINDER_H_
#define ASH_PUBLIC_CPP_WINDOW_FINDER_H_

#include <set>

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ash {

// Finds the topmost window at |screen_point| with ignoring |ignore|. If
// overview is active when this function is called, the overview window that
// contains |screen_point| will be returned. Note this overview window might not
// be visibile (e.g., it represents an aura window whose window state is
// MINIMIZED).
ASH_EXPORT aura::Window* GetTopmostWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<aura::Window*>& ignore);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_FINDER_H_
