// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ash {

namespace capture_mode_util {

// Retrieves the point on the |rect| associated with |position|.
ASH_EXPORT gfx::Point GetLocationForFineTunePosition(const gfx::Rect& rect,
                                                     FineTunePosition position);

// Return whether |position| is a corner.
ASH_EXPORT bool IsCornerFineTunePosition(FineTunePosition position);

}  // namespace capture_mode_util

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_UTIL_H_
