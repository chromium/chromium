// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TRACKED_ELEMENT_RECTS_H_
#define CC_TREES_TRACKED_ELEMENT_RECTS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/token.h"
#include "cc/cc_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

using TrackedElementId = base::Token;

// The feature that is tracking the element. The feature values are kept opaque
// at this level. The actual values are maintained in the higher level browser
// code. If any new features are added, TRACKED_ELEMENT_FEATURE_MAX should be
// updated. For the actual values, see
// components/page_content_annotations/core/tracked_element_feature.h
enum TrackedElementFeature : int32_t {
  TRACKED_ELEMENT_FEATURE_MAX = 1,
};

// New struct to hold the tracked element clipped/visible bounds and other data.
struct CC_EXPORT TrackedElementRect {
  // The id of the element being tracked.
  TrackedElementId id;

  // Visible screen space bounds, clipped against layer visible surface and root
  // surface
  gfx::Rect visible_bounds;

  friend bool operator==(const TrackedElementRect&,
                         const TrackedElementRect&) = default;

  std::string ToString() const;
};

// TrackedElementRects maps precisely to the same-named mojom class, and is
// used to map a "feature" enum to the list of tracked elements being tracked by
// that feature. The element rectangle represents the region of the viewport
// where the element is rendered in a frame.
using TrackedElementRects =
    absl::flat_hash_map<TrackedElementFeature, std::vector<TrackedElementRect>>;

CC_EXPORT std::string TrackedElementRectsToString(
    const TrackedElementRects& rects);

// Returns a reference to a global empty TrackedElementRects. This should
// only be used for functions that need to return a reference to a
// TrackedElementRects, not instead of the default constructor.
CC_EXPORT const TrackedElementRects& TrackedElementRectsEmpty();

}  // namespace cc

#endif  // CC_TREES_TRACKED_ELEMENT_RECTS_H_
