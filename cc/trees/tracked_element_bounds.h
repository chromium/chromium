// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TRACKED_ELEMENT_BOUNDS_H_
#define CC_TREES_TRACKED_ELEMENT_BOUNDS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/token.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

using TrackedElementId = base::Token;

// New struct to hold the tracked element clipped/visible bounds (and more in
// the near future).
struct CC_EXPORT TrackedElementRectData {
  // Visible screen space bounds, clipped against layer visible surface and root
  // surface
  gfx::Rect visible_bounds;

  friend bool operator==(const TrackedElementRectData&,
                         const TrackedElementRectData&) = default;
  friend bool operator!=(const TrackedElementRectData&,
                         const TrackedElementRectData&) = default;
};

// TrackedElementBounds maps precisely to the same-named mojom class, and is
// used for passing in tracked element IDs mapped to a gfx::Rect
// representing the region of the viewport for tracked elements.
using TrackedElementBounds =
    base::flat_map<TrackedElementId, TrackedElementRectData>;

CC_EXPORT std::string TrackedElementBoundsToString(
    const TrackedElementBounds& bounds);

// Returns a reference to a global empty TrackedElementBounds. This should
// only be used for functions that need to return a reference to a
// TrackedElementBounds, not instead of the default constructor.
CC_EXPORT const TrackedElementBounds& TrackedElementBoundsEmpty();

}  // namespace cc

#endif  // CC_TREES_TRACKED_ELEMENT_BOUNDS_H_
