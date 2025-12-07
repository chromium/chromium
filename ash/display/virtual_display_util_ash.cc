// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/gfx/geometry/size.h"

namespace display::test {

// static
std::unique_ptr<VirtualDisplayUtil> VirtualDisplayUtil::TryCreate(
    Screen* screen) {
  return std::make_unique<DisplayManagerTestApi>(
      ash::Shell::Get()->display_manager());
}

}  // namespace display::test
