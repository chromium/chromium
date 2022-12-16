// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/util.h"

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

}  // namespace arc::input_overlay
