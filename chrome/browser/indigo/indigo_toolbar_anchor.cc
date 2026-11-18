// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_toolbar_anchor.h"

#include <algorithm>

#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "ui/gfx/geometry/rect.h"

namespace indigo {

namespace {

// A copy narrower than a valid primary isn't used as an anchor.
constexpr int kMinAnchorCopyWidthDips = kMinPrimaryImageWidthDips;

// A copy clearly larger than the primary that overlaps it (e.g. a zoom view)
// is what the user is looking at, so the toolbar follows it instead.
constexpr double kCopyOverPrimaryAreaRatio = 1.5;

// Returns the largest visible tracked replacement in `rects` that is wide
// enough to be primary and, if `must_intersect` is set, intersects it. Returns
// nullptr if there is none. The width check uses the visible (clipped) width.
// On equal areas the first rect in `rects` wins, which isn't necessarily the
// frontmost.
const viz::TrackedElementRect* FindLargestVisibleCopy(
    base::span<const viz::TrackedElementRect> rects,
    float device_scale_factor,
    const absl::flat_hash_set<base::Token>& tracked_ids,
    const gfx::Rect* must_intersect) {
  const float min_width = kMinAnchorCopyWidthDips * device_scale_factor;
  const viz::TrackedElementRect* largest = nullptr;
  for (const viz::TrackedElementRect& rect : rects) {
    if (rect.visible_bounds.IsEmpty() ||
        rect.visible_bounds.width() < min_width ||
        (must_intersect && !rect.visible_bounds.Intersects(*must_intersect)) ||
        !tracked_ids.contains(rect.id)) {
      continue;
    }
    if (!largest || rect.visible_bounds.size().Area64() >
                        largest->visible_bounds.size().Area64()) {
      largest = &rect;
    }
  }
  return largest;
}

}  // namespace

const viz::TrackedElementRect* SelectToolbarAnchor(
    base::span<const viz::TrackedElementRect> rects,
    const std::optional<base::Token>& primary_token,
    float device_scale_factor,
    const absl::flat_hash_set<base::Token>& tracked_ids) {
  const viz::TrackedElementRect* primary = nullptr;
  if (primary_token) {
    auto it =
        std::ranges::find(rects, *primary_token, &viz::TrackedElementRect::id);
    if (it != rects.end()) {
      primary = &*it;
    }
  }
  // A primary that's painted but clipped off-screen hides the toolbar instead
  // of moving it to a copy, so the toolbar keeps tracking the primary.
  // TODO(b/556246330): A fixed overlay over the primary (e.g. a zoom
  // viewer) also empties its bounds; expose visible_bounds_ignoring_occlusion.
  if (primary && primary->visible_bounds.IsEmpty()) {
    return primary;
  }
  // Every replacement shows the same photo, so any large visible copy works.
  if (!primary) {
    // TODO(b/556246330): Once the primary is removed, Regenerate and
    // the size checks don't apply to this copy; consider promoting it.
    return FindLargestVisibleCopy(rects, device_scale_factor, tracked_ids,
                                  /*must_intersect=*/nullptr);
  }
  // TODO(b/556246330): Rects carry no usable stacking order, so a larger copy
  // behind the primary also wins.
  const viz::TrackedElementRect* overlapping_copy = FindLargestVisibleCopy(
      rects, device_scale_factor, tracked_ids, &primary->visible_bounds);
  if (overlapping_copy &&
      static_cast<double>(overlapping_copy->visible_bounds.size().Area64()) >
          kCopyOverPrimaryAreaRatio *
              static_cast<double>(primary->visible_bounds.size().Area64())) {
    return overlapping_copy;
  }
  return primary;
}

}  // namespace indigo
