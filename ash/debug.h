// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DEBUG_H_
#define ASH_DEBUG_H_

#include "ash/ash_export.h"

namespace ash {
namespace debug {

// Toggles debugging features controlled by
// cc::LayerTreeDebugState.
ASH_EXPORT void ToggleShowDebugBorders();
ASH_EXPORT void ToggleShowFpsCounter();
ASH_EXPORT void ToggleShowPaintRects();

}  // namespace debug
}  // namespace ash

#endif  // ASH_DEBUG_H_
