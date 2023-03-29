// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_DAMAGE_DATA_H_
#define CC_SLIM_DAMAGE_DATA_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include "ui/gfx/geometry/rect.h"

namespace cc::slim {

// Data used to compute damage.
struct DamageData {
  DamageData();
  DamageData(bool property_changed, gfx::Rect visible_rect_in_target);
  ~DamageData();

  // If something that could have caused the content of the layer or render pass
  // to change or shift.
  bool property_changed = true;
  // The visible rect of a layer or render pass in the target space.
  // This may be affected by occlusion, but should not be affected by damage
  // since it is used to compute damage.
  gfx::Rect visible_rect_in_target;
};

// For a particular render pass, store the layer or render pass ID and the
// damage data. Slim compositor render pass id is the same as the root layer
// in that render pass, which means there is never a conflict in this map
// between layers and render passes that draws into a particular render pass.
// Note this is meant to be a map similar to be used as a map and is
// specifically defined as the storage container for a flap_map. Using a vector
// instead since it's not necessary to always keep it sorted; sorting at the end
// after all data has been collected is good enough.
using RenderPassDamageData = std::vector<std::pair<int, DamageData>>;

// Map from render pass ID its damage data.
using FrameDamageData = std::unordered_map<uint64_t, RenderPassDamageData>;

void SortRenderPassDamageData(RenderPassDamageData& data);

}  // namespace cc::slim

#endif  // CC_SLIM_DAMAGE_DATA_H_
