// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

KeyboardPrefHandlerImpl::KeyboardPrefHandlerImpl() = default;
KeyboardPrefHandlerImpl::~KeyboardPrefHandlerImpl() = default;

// TODO(dpad): Implement keyboard settings initialization.
void KeyboardPrefHandlerImpl::InitializeKeyboardSettings(
    PrefService* pref_service,
    mojom::Keyboard* keyboard) {
  NOTIMPLEMENTED();
}

// TODO(dpad): Implement keyboard settings updates.
void KeyboardPrefHandlerImpl::UpdateKeyboardSettings(
    PrefService* pref_service,
    const mojom::Keyboard& keyboard) {
  NOTIMPLEMENTED();
}

}  // namespace ash
