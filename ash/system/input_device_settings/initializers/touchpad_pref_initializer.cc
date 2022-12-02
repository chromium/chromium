// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/touchpad_pref_initializer.h"

#include "base/notreached.h"

namespace ash {

TouchpadPrefInitializer::TouchpadPrefInitializer() = default;
TouchpadPrefInitializer::~TouchpadPrefInitializer() = default;

void TouchpadPrefInitializer::Initialize(PrefService* prefs,
                                         const base::StringPiece& device_key) {
  // TODO(dpad@): Implement touchpad pref initialization.
  NOTIMPLEMENTED();
}

}  // namespace ash
