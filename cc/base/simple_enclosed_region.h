// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_SIMPLE_ENCLOSED_REGION_H_
#define CC_BASE_SIMPLE_ENCLOSED_REGION_H_

#include <stddef.h>

#include <string>

#include "cc/base/base_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class Region;

// A constant-sized approximation of a Region. The SimpleEnclosedRegion may
// exclude points in its approximation (may have false negatives) but will never
// include a point that would not be in the actual Region (no false positives).
class CC_BASE_EXPORT SimpleEnclosedRegion {
 public:
  SimpleEnclosedRegion() : rect_() {}
  SimpleEnclosedRegion(const SimpleEnclosedRegion& region)
      : rect_(region.rect_) {}
  explicit SimpleEnclosedRegion(const gfx::Rect& rect) : rect_(rect) {}
  SimpleEnclosedRegion(int x, int y, int w, int h) : rect_(x, y, w, h) {}
  SimpleEnclosedRegion(int w, int h) : rect_(w, h) {}
  explicit SimpleEnclosedRegion(const Region& region);
  ~SimpleEnclosedRegion();

  const SimpleEnclosedRegion& operator=(const gfx::Rect& rect) {
    rect_ = rect;
    return *this;
  }
  const SimpleEnclosedRegion& operator=(const SimpleEnclosedRegion& region) {
    rect_ = region.rect_;
    return *this;
  }

  bool IsEmpty() const { return rect_.IsEmpty(); }
  void Clear() { rect_ = gfx::Rect(); }
  size_t GetRegionComplexity() const { return rect_.IsEmpty() ? 0 : 1; }

  bool Contains(const gfx::Point& point) const { return rect_.Contains(point); }
  bool Contains(const gfx::Rect& rect) const { return rect_.Contains(rect); }
  bool Contains(const SimpleEnclosedRegion& region) const {
    return rect_.Contains(region.rect_);
  }

  bool Intersects(const gfx::Rect& rect) const {
    return rect_.Intersects(rect);
  }
  bool Intersects(const SimpleEnclosedRegion& region) const {
    return rect_.Intersects(region.rect_);
  }

  void Subtract(const gfx::Rect& sub_rect);
  void Subtract(const SimpleEnclosedRegion& sub_region) {
    Subtract(sub_region.rect_);
  }
  void Union(const gfx::Rect& new_rect);
  void Union(const SimpleEnclosedRegion& new_region) {
    Union(new_region.rect_);
  }
  void Intersect(const gfx::Rect& in_rect) { return rect_.Intersect(in_rect); }
  void Intersect(const SimpleEnclosedRegion& in_region) {
    Intersect(in_region.rect_);
  }

  bool Equals(const SimpleEnclosedRegion& other) const {
    bool both_empty = rect_.IsEmpty() && other.rect_.IsEmpty();
    return both_empty || rect_ == other.rect_;
  }

  gfx::Rect bounds() const { return rect_; }

  // The value of |i| must be less than GetRegionComplexity().
  gfx::Rect GetRect(size_t i) const;

  std::string ToString() const { return rect_.ToString(); }

 private:
  gfx::Rect rect_;
};

inline bool operator==(const SimpleEnclosedRegion& a,
                       const SimpleEnclosedRegion& b) {
  return a.Equals(b);
}

inline bool operator!=(const SimpleEnclosedRegion& a,
                       const SimpleEnclosedRegion& b) {
  return !(a == b);
}

inline SimpleEnclosedRegion SubtractSimpleEnclosedRegions(
    const SimpleEnclosedRegion& a,
    const SimpleEnclosedRegion& b) {
  SimpleEnclosedRegion result = a;
  result.Subtract(b);
  return result;
}

inline SimpleEnclosedRegion IntersectSimpleEnclosedRegions(
    const SimpleEnclosedRegion& a,
    const SimpleEnclosedRegion& b) {
  SimpleEnclosedRegion result = a;
  result.Intersect(b);
  return result;
}

inline SimpleEnclosedRegion UnionSimpleEnclosedRegions(
    const SimpleEnclosedRegion& a,
    const SimpleEnclosedRegion& b) {
  SimpleEnclosedRegion result = a;
  result.Union(b);
  return result;
}

}  // namespace cc

#endif  // CC_BASE_SIMPLE_ENCLOSED_REGION_H_
