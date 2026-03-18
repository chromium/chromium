// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_TRACKED_ELEMENT_RECTS_MOJOM_TRAITS_H_
#define CC_MOJOM_TRACKED_ELEMENT_RECTS_MOJOM_TRAITS_H_

#include <map>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "cc/mojom/tracked_element_rects.mojom-shared.h"
#include "cc/trees/tracked_element_rects.h"
#include "mojo/public/cpp/base/token_mojom_traits.h"
#include "mojo/public/cpp/bindings/map_traits_absl.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::TrackedElementRectDataView,
                    cc::TrackedElementRect> {
  static base::Token tracked_element_id(const cc::TrackedElementRect& data) {
    return data.id;
  }

  static const gfx::Rect& visible_bounds(const cc::TrackedElementRect& data) {
    return data.visible_bounds;
  }

  static bool Read(cc::mojom::TrackedElementRectDataView data,
                   cc::TrackedElementRect* out) {
    if (!data.ReadTrackedElementId(&out->id) ||
        !data.ReadVisibleBounds(&out->visible_bounds)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<cc::mojom::TrackedElementRectsDataView,
                    cc::TrackedElementRects> {
  // TODO: 491757139 - Simplify the serialization/deserialization to avoid
  // making a copy of the map. Also see: crbug.com/40752610.
  static absl::flat_hash_map<int32_t, std::vector<cc::TrackedElementRect>>
  element_data(const cc::TrackedElementRects& data) {
    absl::flat_hash_map<int32_t, std::vector<cc::TrackedElementRect>> map;
    for (const auto& [feature, rects] : data) {
      map.emplace(static_cast<int32_t>(feature), rects);
    }
    return map;
  }

  static bool Read(cc::mojom::TrackedElementRectsDataView data,
                   cc::TrackedElementRects* out) {
    absl::flat_hash_map<int32_t, std::vector<cc::TrackedElementRect>> int_map;
    if (!data.ReadElementData(&int_map)) {
      return false;
    }
    out->reserve(int_map.size());
    for (const auto& [int_val, rects] : int_map) {
      out->emplace(static_cast<cc::TrackedElementFeature>(int_val),
                   std::move(rects));
    }
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_TRACKED_ELEMENT_RECTS_MOJOM_TRAITS_H_
