// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/input_device_settings_controller.h"

namespace ash {

namespace {
InputDeviceSettingsController* g_instance = nullptr;
}

template <>
InputDeviceSettingsController*& InputDeviceSettingsController::
    ScopedResetterForTest::GetGlobalInstanceHolder() {
  return g_instance;
}

InputDeviceSettingsController::InputDeviceSettingsController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

InputDeviceSettingsController::~InputDeviceSettingsController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
InputDeviceSettingsController* InputDeviceSettingsController::Get() {
  return g_instance;
}

}  // namespace ash
