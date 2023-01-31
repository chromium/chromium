// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/util.h"

#include "ash/constants/ash_features.h"
#include "base/notreached.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"

namespace arc::input_overlay {

bool UpdatePositionByArrowKey(ui::KeyboardCode key, gfx::Point& position) {
  switch (key) {
    case ui::VKEY_LEFT:
      position.set_x(position.x() - kArrowKeyMoveDistance);
      return true;
    case ui::VKEY_RIGHT:
      position.set_x(position.x() + kArrowKeyMoveDistance);
      return true;
    case ui::VKEY_UP:
      position.set_y(position.y() - kArrowKeyMoveDistance);
      return true;
    case ui::VKEY_DOWN:
      position.set_y(position.y() + kArrowKeyMoveDistance);
      return true;
    default:
      return false;
  }
}

InputElement* GetInputBindingByBindingOption(Action* action,
                                             BindingOption binding_option) {
  InputElement* input_binding = nullptr;
  switch (binding_option) {
    case BindingOption::kCurrent:
      input_binding = action->current_input();
      break;
    case BindingOption::kOriginal:
      input_binding = action->original_input();
      break;
    case BindingOption::kPending:
      input_binding = action->pending_input();
      break;
    default:
      NOTREACHED();
  }
  return input_binding;
}

bool AllowReposition() {
  return ash::features::IsArcInputOverlayAlphaV2Enabled() ||
         ash::features::IsArcInputOverlayBetaEnabled();
}

}  // namespace arc::input_overlay
