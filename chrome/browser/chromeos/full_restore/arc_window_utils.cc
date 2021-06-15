// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_window_utils.h"

#include "components/arc/arc_util.h"
#include "components/exo/wm_helper.h"
#include "components/full_restore/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

void ScaleToRoundedRect(apps::mojom::Rect* rect, double scale_factor) {
  if (rect == nullptr)
    return;

  auto res_rect = gfx::ScaleToRoundedRect(
      gfx::Rect(rect->x, rect->y, rect->width, rect->height), scale_factor);
  rect->x = res_rect.x();
  rect->y = res_rect.y();
  rect->width = res_rect.width();
  rect->height = res_rect.height();
}

}  // namespace

namespace chromeos {
namespace full_restore {

bool IsArcGhostWindowEnabled() {
  return ::full_restore::features::IsArcGhostWindowEnabled() &&
         arc::IsArcVmEnabled() && exo::WMHelper::HasInstance();
}

absl::optional<double> GetDisplayScaleFactor(int64_t display_id) {
  display::Display display;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display)) {
    return display.device_scale_factor();
  }
  return absl::nullopt;
}

apps::mojom::WindowInfoPtr HandleArcWindowInfo(
    apps::mojom::WindowInfoPtr window_info) {
  // Remove ARC bounds info if the ghost window disabled. The bounds will
  // be controlled by ARC.
  if (!IsArcGhostWindowEnabled()) {
    window_info->bounds.reset();
    return window_info;
  }
  auto scale_factor = GetDisplayScaleFactor(window_info->display_id);

  // Remove ARC bounds info if the the display doesn't exist. The bounds will
  // be controlled by ARC.
  if (!scale_factor.has_value()) {
    window_info->bounds.reset();
    return window_info;
  }

  ScaleToRoundedRect(window_info->bounds.get(), scale_factor.value());
  return window_info;
}

bool IsValidThemeColor(uint32_t theme_color) {
  return SkColorGetA(theme_color) == SK_AlphaOPAQUE;
}

const std::string WindowIdToAppId(int window_id) {
  return std::string("org.chromium.arc.session.") +
         base::NumberToString(window_id);
}

}  // namespace full_restore
}  // namespace chromeos
