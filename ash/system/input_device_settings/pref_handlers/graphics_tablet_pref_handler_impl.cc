// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

GraphicsTabletPrefHandlerImpl::GraphicsTabletPrefHandlerImpl() = default;
GraphicsTabletPrefHandlerImpl::~GraphicsTabletPrefHandlerImpl() = default;

// TODO(wangdanny): Implement graphics_tablet settings initialization.
void GraphicsTabletPrefHandlerImpl::InitializeGraphicsTabletSettings(
    PrefService* pref_service,
    mojom::GraphicsTablet* graphics_tablet) {
  NOTIMPLEMENTED();
}

// TODO(wangdanny): Implement graphics_tablet settings updates.
void GraphicsTabletPrefHandlerImpl::UpdateGraphicsTabletSettings(
    PrefService* pref_service,
    const mojom::GraphicsTablet& graphics_tablet) {
  NOTIMPLEMENTED();
}

}  // namespace ash
