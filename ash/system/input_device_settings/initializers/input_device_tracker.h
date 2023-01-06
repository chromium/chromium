// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_H_

#include "base/strings/string_piece.h"

class PrefService;

namespace ash {

// Used to denote the category of a given input device.
enum class InputDeviceCategory {
  kMouse,
  kTouchpad,
  kPointingStick,
  kKeyboard,
};

// Store observed connected input devices in prefs to be used during the
// transition period from global settings to per-device input settings.
// TODO(dpad@): Remove once transitioned to per-device settings.
class InputDeviceTracker {
 public:
  virtual ~InputDeviceTracker() = default;

  // Initializes the tracker to write updates to a new `PrefService`.
  virtual void Init(PrefService* pref_service) = 0;

  // Records that the given `device_key` has been seen in the correct
  // `category`.
  virtual void RecordDeviceConnected(InputDeviceCategory category,
                                     const base::StringPiece& device_key) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_INPUT_DEVICE_TRACKER_H_
