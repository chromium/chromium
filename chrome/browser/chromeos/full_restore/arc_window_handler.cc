// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_window_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "components/arc/arc_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos {
namespace full_restore {

bool IsArcGhostWindowEnabled() {
  return ash::features::IsFullRestoreEnabled() && arc::IsArcVmEnabled();
}

apps::mojom::WindowInfoPtr ConvertToArcBounds(
    int64_t display_id,
    apps::mojom::WindowInfoPtr window_info) {
  if (!IsArcGhostWindowEnabled()) {
    window_info->bounds.reset();
    return window_info;
  }
  double scale_factor = 0;
  display::Display display;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display)) {
    scale_factor = display.device_scale_factor();
  }
  if (scale_factor == 0) {
    window_info->bounds.reset();
    return window_info;
  }
  // TODO(sstan): Add position adjustment after specify how to conversion
  // to ARC bounds.
  window_info->bounds->width *= scale_factor;
  window_info->bounds->height *= scale_factor;
  return window_info;
}

}  // namespace full_restore
}  // namespace chromeos
