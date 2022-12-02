// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_KEYBOARD_PREF_INITIALIZER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_KEYBOARD_PREF_INITIALIZER_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/initializers/pref_initializer.h"

namespace ash {

// Responsible for initializing keyboard device prefs with default values.
class ASH_EXPORT KeyboardPrefInitializer : public PrefInitializer {
 public:
  KeyboardPrefInitializer();
  KeyboardPrefInitializer(const KeyboardPrefInitializer&) = delete;
  KeyboardPrefInitializer& operator=(const KeyboardPrefInitializer&) = delete;
  ~KeyboardPrefInitializer() override;

  // PrefInitializer:
  void Initialize(PrefService* prefs,
                  const base::StringPiece& device_key) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_KEYBOARD_PREF_INITIALIZER_H_
