// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/region.h"

#include <stddef.h>
#include <utility>

#include "base/no_destructor.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/simple_enclosed_region.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {

Region::Region() = default;

Region::Region(const SkRegion& region) : skregion_(region) {}

Region::Region(const Region& region) = default;

Region::Region(const gfx::Rect& rect)
    : skregion_(gfx::RectToSkIRect(rect)) {
}

Region::~Region() = default;

const Region& Region::operator=(const gfx::Rect& rect) {
  skregion_ = SkRegion(gfx::RectToSkIRect(rect));
  return *this;
}

const Region& Region::operator=(const Region& region) {
  skregion_ = region.skregion_;
  return *this;
}

const Region& Region::operator+=(const gfx::Vector2d& offset) {
  skregion_.translate(offset.x(), offset.y());
  return *this;
}

// static
const Region& Region::Empty() {
  static base::NoDestructor<Region> kEmpty;
  return *kEmpty;
}

void Region::Swap(Region* region) {
  region->skregion_.swap(skregion_);
}

void Region::Clear() {
  skregion_.setEmpty();
}

bool Region::IsEmpty() const {
  return skregion_.isEmpty();
}

int Region::GetRegionComplexity() const {
  return skregion_.computeRegionComplexity();
}

void Region::GetBoundaryPath(SkPath* path) const {
  skregion_.getBoundaryPath(path);
}

bool Region::Contains(const gfx::Point& point) const {
  return skregion_.contains(point.x(), point.y());
}

bool Region::Contains(const gfx::Rect& rect) const {
  if (rect.IsEmpty())
    return true;
  return skregion_.contains(gfx::RectToSkIRect(rect));
}

bool Region::Contains(const Region& region) const {
  if (region.IsEmpty())
    return true;
  return skregion_.contains(region.skregion_);
}

bool Region::Intersects(const gfx::Rect& rect) const {
  return skregion_.intersects(gfx::RectToSkIRect(rect));
}

bool Region::Intersects(const Region& region) const {
  return skregion_.intersects(region.skregion_);
}

void Region::Subtract(const gfx::Rect& rect) {
  skregion_.op(gfx::RectToSkIRect(rect), SkRegion::kDifference_Op);
}

void Region::Subtract(const Region& region) {
  skregion_.op(region.skregion_, SkRegion::kDifference_Op);
}

void Region::Subtract(const SimpleEnclosedRegion& region) {
  for (size_t i = 0; i < region.GetRegionComplexity(); ++i) {
    skregion_.op(gfx::RectToSkIRect(region.GetRect(i)),
                 SkRegion::kDifference_Op);
  }
}

void Region::Union(const gfx::Rect& rect) {
  skregion_.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
}

void Region::Union(const Region& region) {
  skregion_.op(region.skregion_, SkRegion::kUnion_Op);
}

void Region::Intersect(const gfx::Rect& rect) {
  skregion_.op(gfx::RectToSkIRect(rect), SkRegion::kIntersect_Op);
}

void Region::Intersect(const Region& region) {
  skregion_.op(region.skregion_, SkRegion::kIntersect_Op);
}

std::string Region::ToString() const {
  if (IsEmpty())
    return gfx::Rect().ToString();

  std::string result;
  for (gfx::Rect rect : *this) {
    if (!result.empty())
      result += " | ";
    result += rect.ToString();
  }
  return result;
}

void Region::AsValueInto(base::trace_event::TracedValue* result) const {
  for (gfx::Rect rect : *this) {
    result->AppendInteger(rect.x());
    result->AppendInteger(rect.y());
    result->AppendInteger(rect.width());
    result->AppendInteger(rect.height());
  }
}

Region::Iterator Region::begin() const {
  return Region::Iterator(*this);
}

Region::Iterator::Iterator(const Region& region)
    : it_(region.skregion_) {
}

}  // namespace cc
