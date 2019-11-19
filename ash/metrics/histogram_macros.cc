// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/histogram_macros.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

bool InTabletMode() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  return tablet_mode_controller && tablet_mode_controller->InTabletMode();
}

}  // namespace ash
