// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_H_

#include "ash/system/input_device_settings/input_device_settings_controller.h"
#include "base/strings/string_piece.h"

namespace ash {

// Store observed connected input devices in prefs to be used during the
// transition period from global settings to per-device input settings.
// TODO(dpad@): Remove once transitioned to per-device settings.
class InputDeviceTracker {
 public:
  virtual void RecordDeviceConnected(InputDeviceCategory category,
                                     const base::StringPiece& device_key) = 0;

 protected:
  virtual ~InputDeviceTracker() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_H_
