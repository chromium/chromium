// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

MousePrefHandlerImpl::MousePrefHandlerImpl() = default;
MousePrefHandlerImpl::~MousePrefHandlerImpl() = default;

// TODO(michaelcheco): Implement mouse settings initialization.
void MousePrefHandlerImpl::InitializeMouseSettings(PrefService* pref_service,
                                                   mojom::Mouse* mouse) {
  NOTIMPLEMENTED();
}

// TODO(michaelcheco): Implement mouse settings updates.
void MousePrefHandlerImpl::UpdateMouseSettings(PrefService* pref_service,
                                               const mojom::Mouse& mouse) {
  NOTIMPLEMENTED();
}

}  // namespace ash
