// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_STATE_TYPE_H_
#define ASH_PUBLIC_CPP_WINDOW_STATE_TYPE_H_

#include <cstdint>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/ui_base_types.h"

namespace ash {

namespace mojom {
enum class WindowStateType;
}

// Utility functions to convert mojom::WindowStateType <-> ui::WindowShowState.
// Note: LEFT/RIGHT MAXIMIZED, AUTO_POSITIONED types will be lost when
// converting to ui::WindowShowState.
ASH_PUBLIC_EXPORT mojom::WindowStateType ToWindowStateType(
    ui::WindowShowState state);
ASH_PUBLIC_EXPORT ui::WindowShowState ToWindowShowState(
    mojom::WindowStateType type);

// Returns true if |type| is FULLSCREEN, PINNED, or TRUSTED_PINNED.
ASH_PUBLIC_EXPORT bool IsFullscreenOrPinnedWindowStateType(
    mojom::WindowStateType type);

// Returns true if |type| is MAXIMIZED, FULLSCREEN, PINNED, or TRUSTED_PINNED.
ASH_PUBLIC_EXPORT bool IsMaximizedOrFullscreenOrPinnedWindowStateType(
    mojom::WindowStateType type);

// Returns true if |type| is MINIMIZED.
ASH_PUBLIC_EXPORT bool IsMinimizedWindowStateType(mojom::WindowStateType type);

ASH_PUBLIC_EXPORT bool IsValidWindowStateType(int64_t value);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_STATE_TYPE_H_
