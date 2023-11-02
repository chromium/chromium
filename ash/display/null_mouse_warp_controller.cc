// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/null_mouse_warp_controller.h"

namespace ash {

bool NullMouseWarpController::WarpMouseCursor(ui::MouseEvent* event) {
  return false;
}

void NullMouseWarpController::SetEnabled(bool enable) {}

}  // namespace ash
