// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_pref_manager_impl.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/notreached.h"

namespace ash {

InputDevicePrefManagerImpl::InputDevicePrefManagerImpl() = default;
InputDevicePrefManagerImpl::~InputDevicePrefManagerImpl() = default;

// TODO(dpad): Implement retrieval of settings when keyboard is initially
// connected.
void InputDevicePrefManagerImpl::InitializeKeyboardSettings(
    mojom::Keyboard* keyboard) {
  NOTIMPLEMENTED();
}

}  // namespace ash
