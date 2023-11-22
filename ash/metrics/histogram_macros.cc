// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/histogram_macros.h"

#include "ui/display/screen.h"

namespace ash {

bool InTabletMode() {
  return display::Screen::GetScreen()->InTabletMode();
}

}  // namespace ash
