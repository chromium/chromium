// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TARGET_PROPERTY_H_
#define CC_TREES_TARGET_PROPERTY_H_

#include <bitset>

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
  // These sentinels must be last
  FIRST_TARGET_PROPERTY = TRANSFORM,
  LAST_TARGET_PROPERTY = CSS_CUSTOM_PROPERTY
};

}  // namespace TargetProperty

// A set of target properties.
using TargetProperties = std::bitset<kMaxTargetPropertyIndex>;

}  // namespace cc

#endif  // CC_TREES_TARGET_PROPERTY_H_
