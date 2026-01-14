// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_TRACKED_ELEMENT_BOUNDS_MOJOM_TRAITS_H_
#define CC_MOJOM_TRACKED_ELEMENT_BOUNDS_MOJOM_TRAITS_H_

#include <utility>
#include <vector>

#include "cc/mojom/tracked_element_bounds.mojom-shared.h"
#include "cc/trees/tracked_element_bounds.h"
#include "mojo/public/cpp/base/token_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// Intermediary C++ struct to match the Mojo struct definition.
struct TrackedElementRectData {
  base::Token tracked_element_id;
  gfx::Rect visible_bounds;
};

template <>
struct StructTraits<cc::mojom::TrackedElementRectDataDataView,
                    TrackedElementRectData> {
  static base::Token tracked_element_id(const TrackedElementRectData& data) {
    return data.tracked_element_id;
  }

  static const gfx::Rect& visible_bounds(const TrackedElementRectData& data) {
    return data.visible_bounds;
  }

  static bool Read(cc::mojom::TrackedElementRectDataDataView data,
                   TrackedElementRectData* out) {
    if (!data.ReadTrackedElementId(&out->tracked_element_id) ||
        !data.ReadVisibleBounds(&out->visible_bounds)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<cc::mojom::TrackedElementBoundsDataView,
                    cc::TrackedElementBounds> {
  static std::vector<TrackedElementRectData> element_data(
      const cc::TrackedElementBounds& element_bounds) {
    std::vector<TrackedElementRectData> out;
    const auto& data_map = element_bounds;
    out.reserve(data_map.size());
    for (const auto& pair : data_map) {
      out.push_back({pair.first, pair.second.visible_bounds});
    }
    return out;
  }

  static bool Read(cc::mojom::TrackedElementBoundsDataView data,
                   cc::TrackedElementBounds* out) {
    std::vector<TrackedElementRectData> element_rect_data_list;
    if (!data.ReadElementData(&element_rect_data_list)) {
      return false;
    }

    base::flat_map<cc::TrackedElementId, cc::TrackedElementRectData>
        element_map;
    for (const auto& item : element_rect_data_list) {
      element_map.emplace(cc::TrackedElementId(item.tracked_element_id),
                          cc::TrackedElementRectData{item.visible_bounds});
    }
    *out = cc::TrackedElementBounds(std::move(element_map));
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_TRACKED_ELEMENT_BOUNDS_MOJOM_TRAITS_H_
