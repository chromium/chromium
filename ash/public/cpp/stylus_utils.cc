// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/stylus_utils.h"

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "ui/display/display.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/point.h"

namespace ash {
namespace stylus_utils {

namespace {
// If true the device performs as if it is hardware reports that it is stylus
// capable.
bool g_has_stylus_input_for_testing = false;
}  // namespace

bool HasForcedStylusInput() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshForceEnableStylusTools);
}

bool HasStylusInput() {
  if (g_has_stylus_input_for_testing)
    return true;

  // Allow the user to force enable or disable by passing a switch. If both are
  // present, enabling takes precedence over disabling.
  if (HasForcedStylusInput())
    return true;

  // Check to see if the hardware reports it is stylus capable.
  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus &&
        device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }

  return false;
}

bool IsPaletteEnabledOnEveryDisplay() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnablePaletteOnAllDisplays);
}

bool HasInternalStylus() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHasInternalStylus);
}

void SetHasStylusInputForTesting() {
  g_has_stylus_input_for_testing = true;
}

}  // namespace stylus_utils
}  // namespace ash
