// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_

namespace gfx {
class Rect;
class RectF;
}  // namespace gfx

namespace aura {
class Window;
}

namespace arc {
// Given a rect in Android pixels, returns a scaled rectangle in Chrome pixels.
// This only scales the given bounds.
gfx::RectF ScaleAndroidPxToChromePx(const gfx::Rect& android_bounds,
                                    aura::Window* window);

// Returns an difference of y coordinate in DIP between Android internal bounds
// and what Chrome actually renders in the screen.
int GetChromeWindowHeightOffsetInDip(aura::Window* window);
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
