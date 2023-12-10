// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_

#include <optional>

#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace aura {
class Window;
}

namespace arc {

bool IsArcOrGhostWindow(const aura::Window* window);

// Finds ARC window from the given window to the parent.
aura::Window* FindArcWindow(aura::Window* child);
aura::Window* FindArcOrGhostWindow(aura::Window* child);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
