// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_
#define ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_

#include "ash/ash_export.h"

namespace ash {

// TODO(jimmyxgong): Determine if this enum can be rolled from Dbus constants.
enum class RgbKeyboardCapabilities {
  kNone,
  kFiveZone,
  kIndividualKey,
};

// RgbKeyboardManager is singleton class that provides clients access to
// RGB keyboard-related API's. Clients should interact with this class instead
// of the rgbkbd DBus client.
// This class is owned by ash/shell and should NOT be created by any other
// means.
class ASH_EXPORT RgbKeyboardManager {
 public:
  RgbKeyboardManager();
  RgbKeyboardManager(const RgbKeyboardManager&) = delete;
  RgbKeyboardManager& operator=(const RgbKeyboardManager&) = delete;
  ~RgbKeyboardManager();

  RgbKeyboardCapabilities GetRgbKeyboardCapabilities() const;

  // Returns the global instance if initialized. May return null.
  static RgbKeyboardManager* Get();
};

}  // namespace ash

#endif  // ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_