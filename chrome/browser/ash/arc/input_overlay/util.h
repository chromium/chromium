// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UTIL_H_

#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"

namespace arc::input_overlay {

class Action;
class InputElement;

// Arrow key move distance per key press event.
constexpr int kArrowKeyMoveDistance = 2;

// Update |position| according to |key| if |key| is arrow key.
bool UpdatePositionByArrowKey(ui::KeyboardCode key, gfx::Point& position);

// Return the input binding filtered by |binding_option| in |action|.
InputElement* GetInputBindingByBindingOption(Action* action,
                                             BindingOption binding_option);

// TODO(b/260937747): Update or remove when removing flags
// |kArcInputOverlayAlphaV2| or |kArcInputOverlayBeta|.
bool AllowReposition();

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UTIL_H_
