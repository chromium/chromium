// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/damage_data.h"

#include <utility>

#include "base/containers/flat_map.h"

namespace cc::slim {

DamageData::DamageData(bool property_changed, gfx::Rect visible_rect_in_target)
    : property_changed(property_changed),
      visible_rect_in_target(visible_rect_in_target) {}

DamageData::~DamageData() = default;

// Sort by converting to flat_map then back.
void SortRenderPassDamageData(RenderPassDamageData& data) {
  data = base::flat_map<int, DamageData>(std::move(data)).extract();
}

}  // namespace cc::slim
