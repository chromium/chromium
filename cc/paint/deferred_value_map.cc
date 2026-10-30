// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_value_map.h"

#include <utility>

namespace cc {

DeferredValueMap::DeferredValueMap() = default;
DeferredValueMap::DeferredValueMap(
    base::flat_map<DeferredPropertyKey, SkColor4f> colors)
    : colors(std::move(colors)) {}
DeferredValueMap::DeferredValueMap(DeferredValueMap&&) noexcept = default;
DeferredValueMap& DeferredValueMap::operator=(DeferredValueMap&&) noexcept =
    default;
DeferredValueMap::~DeferredValueMap() = default;

DeferredValueMap DeferredValueMap::Clone() const {
  // Unpack `*this` so adding a member fails to compile until cloned here.
  const auto& [cloned_colors] = *this;
  return DeferredValueMap(cloned_colors);
}

}  // namespace cc
