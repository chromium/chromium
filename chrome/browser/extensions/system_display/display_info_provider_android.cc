// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_android.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/display/display.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

DisplayInfoProviderAndroid::DisplayInfoProviderAndroid() = default;

DisplayInfoProviderAndroid::~DisplayInfoProviderAndroid() = default;

void DisplayInfoProviderAndroid::UpdateDisplayUnitInfoForPlatform(
    const std::vector<display::Display>& displays,
    DisplayUnitInfoList& units) const {
  for (size_t display_index = 0; display_index < displays.size();
       ++display_index) {
    const display::Display& display = displays[display_index];
    units[display_index].name = display.label();
    if (units[display_index].name.empty()) {
      units[display_index].name = base::NumberToString(display.id());
    }

    double dpi_x = display.GetPixelsPerInchX();
    double dpi_y = display.GetPixelsPerInchY();
    if (dpi_x <= 0 || dpi_y <= 0) {
      const double fallback_scale = display.device_scale_factor() > 0
                                        ? display.device_scale_factor()
                                        : 1.0;
      const double fallback_dpi = 96.0 * fallback_scale;
      dpi_x = dpi_x > 0 ? dpi_x : fallback_dpi;
      dpi_y = dpi_y > 0 ? dpi_y : fallback_dpi;
    }
    units[display_index].dpi_x = dpi_x;
    units[display_index].dpi_y = dpi_y;
  }
}

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  return std::make_unique<DisplayInfoProviderAndroid>();
}

}  // namespace extensions
