// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_TOOLBAR_ANCHOR_H_
#define CHROME_BROWSER_INDIGO_INDIGO_TOOLBAR_ANCHOR_H_

#include <optional>

#include "base/containers/span.h"
#include "base/token.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace viz {
struct TrackedElementRect;
}  // namespace viz

namespace indigo {

// The minimum width of the primary image frame in DIPs below which
// the transformation will fail.
inline constexpr int kMinPrimaryImageWidthDips = 170;

// Returns the rect in `rects` the toolbar should track, or nullptr if there is
// none. `primary_token` is the tracked element id of the primary replacement
// and `tracked_ids` holds the ids of all live replacements. The returned
// pointer points into `rects`.
const viz::TrackedElementRect* SelectToolbarAnchor(
    base::span<const viz::TrackedElementRect> rects,
    const std::optional<base::Token>& primary_token,
    float device_scale_factor,
    const absl::flat_hash_set<base::Token>& tracked_ids);

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_TOOLBAR_ANCHOR_H_
