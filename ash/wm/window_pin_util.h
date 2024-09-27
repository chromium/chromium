// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PIN_UTIL_H_
#define ASH_WM_WINDOW_PIN_UTIL_H_

#include "ash/ash_export.h"
#include "chromeos/ui/base/window_pin_type.h"

namespace aura {
class Window;
}

// Sets the window state to pinned.  If |trusted| is true, sets to
// kTrustedPinned.  Otherwise, the window is set to kPinned.
ASH_EXPORT void PinWindow(aura::Window* window, bool trusted);

// Revert the window state to default from a pinned state.
ASH_EXPORT void UnpinWindow(aura::Window* window);

// Returns the pinned state for a window.
ASH_EXPORT chromeos::WindowPinType GetWindowPinType(const aura::Window* window);

// Returns true if the window is Pinned or TrustedPinned.
ASH_EXPORT bool IsWindowPinned(const aura::Window* window);

#endif  // ASH_WM_WINDOW_PIN_UTIL_H_
