// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_colors.h"

namespace ash {

HoldingSpaceColors::HoldingSpaceColors(SkColor dark_mode, SkColor light_mode)
    : dark_mode_(dark_mode), light_mode_(light_mode) {}

HoldingSpaceColors::HoldingSpaceColors(const HoldingSpaceColors&) = default;

HoldingSpaceColors& HoldingSpaceColors::operator=(const HoldingSpaceColors&) =
    default;

HoldingSpaceColors::~HoldingSpaceColors() = default;

bool HoldingSpaceColors::operator==(const HoldingSpaceColors& rhs) const =
    default;

}  // namespace ash
