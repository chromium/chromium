// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_tray_test_api.h"

#include "base/check.h"

namespace ash {

PaletteTrayTestApi::PaletteTrayTestApi(PaletteTray* palette_tray)
    : palette_tray_(palette_tray) {
  DCHECK(palette_tray_);
}

PaletteTrayTestApi::~PaletteTrayTestApi() = default;

}  // namespace ash
