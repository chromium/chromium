// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/layer_selection_bound_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    cc::mojom::LayerSelectionBoundDataView,
    cc::LayerSelectionBound>::Read(cc::mojom::LayerSelectionBoundDataView data,
                                   cc::LayerSelectionBound* out) {
  if (!data.ReadType(&out->type) || !data.ReadEdgeStart(&out->edge_start) ||
      !data.ReadEdgeEnd(&out->edge_end)) {
    return false;
  }
  out->layer_id = data.layer_id();
  out->hidden = data.hidden();
  return true;
}

// static
bool StructTraits<cc::mojom::LayerSelectionDataView, cc::LayerSelection>::Read(
    cc::mojom::LayerSelectionDataView data,
    cc::LayerSelection* out) {
  return data.ReadStart(&out->start) && data.ReadEnd(&out->end);
}

}  // namespace mojo
