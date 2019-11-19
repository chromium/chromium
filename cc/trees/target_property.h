// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TARGET_PROPERTY_H_
#define CC_TREES_TARGET_PROPERTY_H_

#include <bitset>

#include "base/containers/flat_map.h"

namespace cc {

static constexpr size_t kMaxTargetPropertyIndex = 32u;

namespace TargetProperty {

// Must be zero-based as this will be stored in a bitset.
enum Type {
  TRANSFORM = 0,
  OPACITY,
  FILTER,
  SCROLL_OFFSET,
  BACKGROUND_COLOR,
  BOUNDS,
  CSS_CUSTOM_PROPERTY,
  BACKDROP_FILTER,
  // These sentinels must be last
  FIRST_TARGET_PROPERTY = TRANSFORM,
  LAST_TARGET_PROPERTY = BACKDROP_FILTER
};

}  // namespace TargetProperty

// A set of target properties.
using TargetProperties = std::bitset<kMaxTargetPropertyIndex>;

// A map of target property to ElementId.
// flat_map was chosen because there are expected to be relatively few entries
// in the map. For low number of entries, flat_map is known to perform better
// than other map implementations.
struct ElementId;
using PropertyToElementIdMap = base::flat_map<TargetProperty::Type, ElementId>;

}  // namespace cc

#endif  // CC_TREES_TARGET_PROPERTY_H_
