// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_win.h"

#include <windows.h>

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

using api::system_display::DisplayUnitInfo;

namespace {

BOOL CALLBACK EnumMonitorCallback(HMONITOR monitor,
                                  HDC hdc,
                                  LPRECT rect,
                                  LPARAM data) {
  base::flat_map<std::string, std::string>* device_id_to_name =
      reinterpret_cast<base::flat_map<std::string, std::string>*>(data);
  DCHECK(device_id_to_name);

  DisplayUnitInfo unit;

  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(monitor, &monitor_info);

  DISPLAY_DEVICE device;
  device.cb = sizeof(device);
  if (!EnumDisplayDevices(monitor_info.szDevice, 0, &device, 0)) {
    return FALSE;
  }

  std::string id = base::NumberToString(
      display::win::ScreenWin::DisplayIdFromMonitorInfo(monitor_info));
  (*device_id_to_name)[id] = base::WideToUTF8(device.DeviceString);
  return TRUE;
}

}  // namespace

DisplayInfoProviderWin::DisplayInfoProviderWin() = default;

void DisplayInfoProviderWin::UpdateDisplayUnitInfoForPlatform(
    const std::vector<display::Display>& displays,
    DisplayUnitInfoList& units) const {
  base::flat_map<std::string, std::string> device_id_to_name;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorCallback,
                      reinterpret_cast<LPARAM>(&device_id_to_name));
  // `displays` and `units` are in the same order. Each unit has an id
  // which is the string representation of the corresponding display's id.
  for (size_t display_index = 0; display_index < displays.size();
       display_index++) {
    auto it = device_id_to_name.find(units[display_index].id);
    if (it != device_id_to_name.end()) {
      units[display_index].name = it->second;
      float device_scale_factor = displays[display_index].device_scale_factor();
      int dpi = display::win::GetDPIFromScalingFactor(device_scale_factor);
      units[display_index].dpi_x = dpi;
      units[display_index].dpi_y = dpi;
    }
  }
}

std::unique_ptr<DisplayInfoProvider> CreateChromeDisplayInfoProvider() {
  return std::make_unique<DisplayInfoProviderWin>();
}

}  // namespace extensions
