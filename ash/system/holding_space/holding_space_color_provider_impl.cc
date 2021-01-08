// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_color_provider_impl.h"

#include "ash/style/ash_color_provider.h"
#include "ui/gfx/color_palette.h"

namespace ash {

HoldingSpaceColorProviderImpl::HoldingSpaceColorProviderImpl() = default;

HoldingSpaceColorProviderImpl::~HoldingSpaceColorProviderImpl() = default;

SkColor HoldingSpaceColorProviderImpl::GetBackgroundColor() const {
  return AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
}

SkColor HoldingSpaceColorProviderImpl::GetFileIconColor() const {
  return gfx::kGoogleGrey700;
}

}  // namespace ash
