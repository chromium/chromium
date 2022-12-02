// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/mouse_pref_initializer.h"

#include "base/notreached.h"

namespace ash {

MousePrefInitializer::MousePrefInitializer() = default;
MousePrefInitializer::~MousePrefInitializer() = default;

void MousePrefInitializer::Initialize(PrefService* prefs,
                                      const base::StringPiece& device_key) {
  // TODO(dpad@): Implement mouse pref initialization.
  NOTIMPLEMENTED();
}

}  // namespace ash
