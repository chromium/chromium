// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_TYPES_UTIL_H_
#define ASH_PUBLIC_CPP_APP_TYPES_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"

namespace aura {
class Window;
}

namespace ash {

// Returns true if `window` is an ARC app window.
ASH_PUBLIC_EXPORT bool IsArcWindow(const aura::Window* window);

// Returns true if `window` is a lacros window.
ASH_PUBLIC_EXPORT bool IsLacrosWindow(const aura::Window* window);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_TYPES_UTIL_H_
