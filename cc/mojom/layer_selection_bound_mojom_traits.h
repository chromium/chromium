// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_LAYER_SELECTION_BOUND_MOJOM_TRAITS_H_
#define CC_MOJOM_LAYER_SELECTION_BOUND_MOJOM_TRAITS_H_

#include "cc/input/layer_selection_bound.h"
#include "cc/mojom/layer_selection_bound.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::LayerSelectionBoundDataView,
                    cc::LayerSelectionBound> {
  static gfx::SelectionBound::Type type(const cc::LayerSelectionBound& bound) {
    return bound.type;
  }

  static const gfx::Point& edge_start(const cc::LayerSelectionBound& bound) {
    return bound.edge_start;
  }

  static const gfx::Point& edge_end(const cc::LayerSelectionBound& bound) {
    return bound.edge_end;
  }

  static int32_t layer_id(const cc::LayerSelectionBound& bound) {
    return bound.layer_id;
  }

  static bool hidden(const cc::LayerSelectionBound& bound) {
    return bound.hidden;
  }

  static bool Read(cc::mojom::LayerSelectionBoundDataView data,
                   cc::LayerSelectionBound* out);
};

template <>
struct StructTraits<cc::mojom::LayerSelectionDataView, cc::LayerSelection> {
  static const cc::LayerSelectionBound& start(
      const cc::LayerSelection& selection) {
    return selection.start;
  }

  static const cc::LayerSelectionBound& end(
      const cc::LayerSelection& selection) {
    return selection.end;
  }

  static bool Read(cc::mojom::LayerSelectionDataView data,
                   cc::LayerSelection* out);
};

}  // namespace mojo

#endif  // CC_MOJOM_LAYER_SELECTION_BOUND_MOJOM_TRAITS_H_
