// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_REGION_H_
#define CC_BASE_REGION_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "cc/base/base_export.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"

class SkPath;

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace gfx {
class Vector2d;
}

namespace cc {
class SimpleEnclosedRegion;

class CC_BASE_EXPORT Region {
 public:
  Region();
  explicit Region(const SkRegion& region);
  Region(const Region& region);
  Region(const gfx::Rect& rect);  // NOLINT(runtime/explicit)
  ~Region();

  const Region& operator=(const gfx::Rect& rect);
  const Region& operator=(const Region& region);
  const Region& operator+=(const gfx::Vector2d& offset);

  // Returns a reference to a global empty Region. This should only be used for
  // functions that need to return a reference to a Region, not instead of the
  // default constructor.
  static const Region& Empty();

  void Swap(Region* region);
  void Clear();
  bool IsEmpty() const;
  int GetRegionComplexity() const;
  void GetBoundaryPath(SkPath* path) const;

  bool Contains(const gfx::Point& point) const;
  bool Contains(const gfx::Rect& rect) const;
  bool Contains(const Region& region) const;

  bool Intersects(const gfx::Rect& rect) const;
  bool Intersects(const Region& region) const;

  void Subtract(const gfx::Rect& rect);
  void Subtract(const Region& region);
  void Subtract(const SimpleEnclosedRegion& region);
  void Union(const gfx::Rect& rect);
  void Union(const Region& region);
  void Intersect(const gfx::Rect& rect);
  void Intersect(const Region& region);

  bool Equals(const Region& other) const {
    return skregion_ == other.skregion_;
  }

  gfx::Rect bounds() const {
    return gfx::SkIRectToRect(skregion_.getBounds());
  }

  std::string ToString() const;
  void AsValueInto(base::trace_event::TracedValue* array) const;

  // Iterator for iterating through the gfx::Rects contained in this Region.
  // We only support forward iteration as the underlying SkRegion::Iterator
  // only supports forward iteration.
  class CC_BASE_EXPORT Iterator {
   public:
    Iterator() = default;
    ~Iterator() = default;

    gfx::Rect operator*() const { return gfx::SkIRectToRect(it_.rect()); }

    Iterator& operator++() {
      it_.next();
      return *this;
    }

    bool operator==(const Iterator& b) const {
      // This should only be used to compare to end().
      DCHECK(b.it_.done());
      return it_.done();
    }

    bool operator!=(const Iterator& b) const { return !(*this == b); }

   private:
    explicit Iterator(const Region& region);
    friend class Region;

    SkRegion::Iterator it_;
  };

  Iterator begin() const;
  Iterator end() const { return Iterator(); }

 private:
  SkRegion skregion_;
};

inline bool operator==(const Region& a, const Region& b) {
  return a.Equals(b);
}

inline bool operator!=(const Region& a, const Region& b) {
  return !(a == b);
}

inline Region operator+(const Region& a, const gfx::Vector2d& b) {
  Region result = a;
  result += b;
  return result;
}

inline Region SubtractRegions(const Region& a, const Region& b) {
  Region result = a;
  result.Subtract(b);
  return result;
}

inline Region SubtractRegions(const Region& a, const gfx::Rect& b) {
  Region result = a;
  result.Subtract(b);
  return result;
}

inline Region IntersectRegions(const Region& a, const Region& b) {
  Region result = a;
  result.Intersect(b);
  return result;
}

inline Region IntersectRegions(const Region& a, const gfx::Rect& b) {
  Region result = a;
  result.Intersect(b);
  return result;
}

inline Region UnionRegions(const Region& a, const Region& b) {
  Region result = a;
  result.Union(b);
  return result;
}

inline Region UnionRegions(const Region& a, const gfx::Rect& b) {
  Region result = a;
  result.Union(b);
  return result;
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //cc:test_support target. Depend on that to use this in your unit test.
// This should not be used in production code - call ToString() instead.
void PrintTo(const Region& region, std::ostream* os);

}  // namespace cc

#endif  // CC_BASE_REGION_H_
