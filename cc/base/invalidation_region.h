// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_INVALIDATION_REGION_H_
#define CC_BASE_INVALIDATION_REGION_H_

#include <vector>

#include "cc/base/base_export.h"
#include "cc/base/region.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

// This class behaves similarly to Region, but it may have false positives. That
// is, InvalidationRegion can be simplified to encompass a larger area than the
// collection of rects unioned.
class CC_BASE_EXPORT InvalidationRegion {
 public:
  InvalidationRegion();
  ~InvalidationRegion();

  void Swap(Region* region);
  void Clear();
  void Union(const gfx::Rect& rect);
  bool IsEmpty() const { return pending_rects_.empty() && region_.IsEmpty(); }

 private:
  void FinalizePendingRects();

  Region region_;
  std::vector<gfx::Rect> pending_rects_;
};

}  // namespace cc

#endif  // CC_BASE_INVALIDATION_REGION_H_
