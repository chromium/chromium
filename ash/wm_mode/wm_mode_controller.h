// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MODE_WM_MODE_CONTROLLER_H_
#define ASH_WM_MODE_WM_MODE_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash {

// Controls an *experimental* feature that allows users to easily layout, resize
// and position their windows using only mouse and touch gestures without having
// to be very precise at dragging, or targeting certain buttons. A demo of an
// exploration prototype can be watched at https://crbug.com/1348416.
// Please note this feature may never be released.
class ASH_EXPORT WmModeController {
 public:
  WmModeController();
  WmModeController(const WmModeController&) = delete;
  WmModeController& operator=(const WmModeController&) = delete;
  ~WmModeController();

  static WmModeController* Get();

  bool is_active() const { return is_active_; }

  // Toggles the active state of this mode.
  void Toggle();

 private:
  bool is_active_ = false;
};

}  // namespace ash

#endif  // ASH_WM_MODE_WM_MODE_CONTROLLER_H_
