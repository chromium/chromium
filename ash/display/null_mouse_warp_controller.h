// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_NULL_MOUSE_WARP_CONTROLLER_H_
#define ASH_DISPLAY_NULL_MOUSE_WARP_CONTROLLER_H_

#include "ash/display/mouse_warp_controller.h"

#include "base/macros.h"

namespace ash {

// A MouseWarpController when there is one desktop display
// (in single display or mirror mode).
class NullMouseWarpController : public MouseWarpController {
 public:
  NullMouseWarpController() {}

  // MouseWarpController:
  bool WarpMouseCursor(ui::MouseEvent* event) override;
  void SetEnabled(bool enable) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullMouseWarpController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_NULL_MOUSE_WARP_CONTROLLER_H_
