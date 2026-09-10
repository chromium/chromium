// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_util.h"

#include "ash/style/ash_color_id.h"
#include "ui/decoration/decoration_util.h"

namespace ash::drag_drop {

namespace {
constexpr int kShadowElevation = 2;
}  // namespace

const ui::ColorId kDragImageBackgroundColor = kColorAshShieldAndBaseOpaque;

const ui::decoration::ShadowDetails& GetDragImageShadowDetails(
    const std::optional<size_t>& corner_radius) {
  return ui::decoration::ShadowDetails::Get(
      kShadowElevation, gfx::RoundedCornersF(corner_radius.value_or(0)));
}

}  // namespace ash::drag_drop
