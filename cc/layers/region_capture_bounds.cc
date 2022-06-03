// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/region_capture_bounds.h"

#include <utility>

namespace cc {

RegionCaptureBounds::RegionCaptureBounds() = default;
RegionCaptureBounds::RegionCaptureBounds(const RegionCaptureBounds& regions) =
    default;
RegionCaptureBounds::RegionCaptureBounds(RegionCaptureBounds&& regions) =
    default;
RegionCaptureBounds::RegionCaptureBounds(
    base::flat_map<RegionCaptureCropId, gfx::Rect> bounds)
    : bounds_(std::move(bounds)) {}
RegionCaptureBounds::~RegionCaptureBounds() = default;

void RegionCaptureBounds::Set(const RegionCaptureCropId& crop_id,
                              const gfx::Rect& region) {
  bounds_.insert_or_assign(crop_id, region);
}

RegionCaptureBounds& RegionCaptureBounds::operator=(
    const RegionCaptureBounds& other) = default;
RegionCaptureBounds& RegionCaptureBounds::operator=(
    RegionCaptureBounds&& other) = default;

bool RegionCaptureBounds::operator==(const RegionCaptureBounds& other) const {
  return bounds_ == other.bounds_;
}

}  // namespace cc
