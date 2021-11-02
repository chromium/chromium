// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_REGION_CAPTURE_BOUNDS_H_
#define CC_LAYERS_REGION_CAPTURE_BOUNDS_H_

#include "base/containers/flat_map.h"
#include "base/token.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

using RegionCaptureCropId = base::Token;

// Represents a map from a region capture crop identifier, which is a randomly
// generated token, to a rectangle representing the bounds of the HTML element
// associated with the crop identifier. The boundaries here are captured from
// the CompositorFrame root render pass and are thus in that coordinate space.
// See the design document at: https://tinyurl.com/region-capture.
class CC_EXPORT RegionCaptureBounds final {
 public:
  RegionCaptureBounds();
  RegionCaptureBounds(const RegionCaptureBounds& regions);
  RegionCaptureBounds(RegionCaptureBounds&& regions);
  explicit RegionCaptureBounds(
      base::flat_map<RegionCaptureCropId, gfx::Rect> bounds);
  ~RegionCaptureBounds();

  // We currently only support a single set of bounds for a given crop id.
  // Multiple calls with the same crop id will update the bounds.
  void Set(const RegionCaptureCropId& crop_id, const gfx::Rect& bounds);

  const base::flat_map<RegionCaptureCropId, gfx::Rect>& bounds() const {
    return bounds_;
  }

  RegionCaptureBounds& operator=(const RegionCaptureBounds& other);
  RegionCaptureBounds& operator=(RegionCaptureBounds&& other);
  bool operator==(const RegionCaptureBounds& other) const;

 private:
  base::flat_map<RegionCaptureCropId, gfx::Rect> bounds_;
};

}  // namespace cc

#endif  // CC_LAYERS_REGION_CAPTURE_BOUNDS_H_
