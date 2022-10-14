// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_OBSERVER_H_
#define ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {

// Used to listen for rgb_keyboard_manager changes.
class ASH_EXPORT RgbKeyboardManagerObserver : public base::CheckedObserver {
 public:
  // Invoked when rgb keyboard support is determined. Called only once after
  // fetching rgb capabilities.
  virtual void OnRgbKeyboardSupportedChanged(bool supported) {}

 protected:
  ~RgbKeyboardManagerObserver() override = default;
};

}  // namespace ash

#endif  // ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_OBSERVER_H_
