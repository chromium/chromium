// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_layout_constants.h"

#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"

namespace ash {

gfx::Size GetAshLayoutSize(AshLayoutSize size) {
  constexpr int kButtonWidth = 32;

  if (size == AshLayoutSize::kNonBrowserCaption)
    return gfx::Size(kButtonWidth, 32);

  // |kBrowserMaximizedCaptionButtonHeight| should be kept in sync with those
  // for TAB_HEIGHT in // chrome/browser/ui/layout_constants.cc.
  // TODO: Ideally these values should be obtained from a common location.
  int height = ui::MaterialDesignController::touch_ui() ? 41 : 34;
  if (size == AshLayoutSize::kBrowserCaptionRestored)
    height += 8;  // Restored window titlebars are 8 DIP taller than maximized.
  return gfx::Size(kButtonWidth, height);
}

}  // namespace ash
