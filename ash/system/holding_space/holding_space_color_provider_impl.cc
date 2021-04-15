// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_color_provider_impl.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"

namespace ash {

HoldingSpaceColorProviderImpl::HoldingSpaceColorProviderImpl() = default;

HoldingSpaceColorProviderImpl::~HoldingSpaceColorProviderImpl() = default;

SkColor HoldingSpaceColorProviderImpl::GetBackgroundColor() const {
  return AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
}

// TODO(crbug.com/1156190): Support dark/light mode.
bool HoldingSpaceColorProviderImpl::IsDarkModeEnabled() const {
  ScopedLightModeAsDefault scoped_light_mode_as_default;
  return AshColorProvider::Get()->IsDarkModeEnabled();
}

}  // namespace ash
