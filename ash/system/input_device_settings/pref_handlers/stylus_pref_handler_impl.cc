// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/stylus_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

StylusPrefHandlerImpl::StylusPrefHandlerImpl() = default;
StylusPrefHandlerImpl::~StylusPrefHandlerImpl() = default;

// TODO(wangdanny): Implement stylus settings initialization.
void StylusPrefHandlerImpl::InitializeStylusSettings(PrefService* pref_service,
                                                     mojom::Stylus* stylus) {
  NOTIMPLEMENTED();
}

// TODO(wangdanny): Implement stylus settings updates.
void StylusPrefHandlerImpl::UpdateStylusSettings(PrefService* pref_service,
                                                 const mojom::Stylus& stylus) {
  NOTIMPLEMENTED();
}

}  // namespace ash
