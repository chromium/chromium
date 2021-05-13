// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_BUBBLE_UTILS_H_
#define ASH_BUBBLE_BUBBLE_UTILS_H_

#include "ash/ash_export.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace ash {
namespace bubble_utils {

// Returns false if `event` should not close a bubble. Returns true if `event`
// should close a bubble, or if more processing is required. Callers may also
// need to check for a click on the view that spawned the bubble (otherwise the
// bubble will close and immediately reopen).
ASH_EXPORT bool ShouldCloseBubbleForEvent(const ui::LocatedEvent& event);

}  // namespace bubble_utils
}  // namespace ash

#endif  // ASH_BUBBLE_BUBBLE_UTILS_H_
