// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TARGET_PROPERTY_H_
#define CC_TREES_TARGET_PROPERTY_H_

#include "base/containers/flat_map.h"

namespace cc {

namespace TargetProperty {

// Must be zero-based as this will be stored in a bitset.
enum Type {
  TRANSFORM = 0,
  SCALE,
  ROTATE,
  TRANSLATE,
  OPACITY,
  FILTER,
  SCROLL_OFFSET,
  BACKGROUND_COLOR,
  BOUNDS,
  CSS_CUSTOM_PROPERTY,
  // This is used for the set of properties whose animation use paint worklet
  // infra. The value of the animation represents its progress.
  NATIVE_PROPERTY,
  BACKDROP_FILTER,
  // These sentinels must be last
  FIRST_TARGET_PROPERTY = TRANSFORM,
  LAST_TARGET_PROPERTY = BACKDROP_FILTER
};

}  // namespace TargetProperty

// A map of target property to ElementId.
// flat_map was chosen because there are expected to be relatively few entries
// in the map. For low number of entries, flat_map is known to perform better
// than other map implementations.
struct ElementId;
using PropertyToElementIdMap = base::flat_map<TargetProperty::Type, ElementId>;

}  // namespace cc

#endif  // CC_TREES_TARGET_PROPERTY_H_
