// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/touch_action_region.h"

#include "ui/gfx/geometry/rect.h"

namespace cc {

TouchActionRegion::TouchActionRegion() {}
TouchActionRegion::TouchActionRegion(
    const TouchActionRegion& touch_action_region) = default;
TouchActionRegion::TouchActionRegion(TouchActionRegion&& touch_action_region) =
    default;

TouchActionRegion::~TouchActionRegion() = default;

Region TouchActionRegion::GetAllRegions() const {
  Region all_regions;
  for (const auto& pair : map_)
    all_regions.Union(pair.second);
  return all_regions;
}

void TouchActionRegion::Union(TouchAction touch_action, const gfx::Rect& rect) {
  map_[touch_action].Union(rect);
}

const Region& TouchActionRegion::GetRegionForTouchAction(
    TouchAction touch_action) const {
  static const Region* empty_region = new Region;
  auto it = map_.find(touch_action);
  if (it == map_.end())
    return *empty_region;
  return it->second;
}

TouchAction TouchActionRegion::GetAllowedTouchAction(
    const gfx::Point& point) const {
  TouchAction allowed_touch_action = kTouchActionAuto;
  for (const auto& pair : map_) {
    if (!pair.second.Contains(point))
      continue;
    allowed_touch_action &= pair.first;
  }
  return allowed_touch_action;
}

TouchActionRegion& TouchActionRegion::operator=(
    const TouchActionRegion& other) {
  map_ = other.map_;
  return *this;
}

TouchActionRegion& TouchActionRegion::operator=(TouchActionRegion&& other) =
    default;

bool TouchActionRegion::operator==(const TouchActionRegion& other) const {
  return map_ == other.map_;
}

}  // namespace cc
