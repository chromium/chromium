// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_LAYER_SELECTION_BOUND_H_
#define CC_INPUT_LAYER_SELECTION_BOUND_H_

#include <string>

#include "cc/cc_export.h"
#include "components/viz/common/quads/selection.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/selection_bound.h"

namespace cc {

// Marker for a selection end-point attached to a specific layer.
// TODO(fsamuel): This could be unified with gfx::SelectionBound.
struct CC_EXPORT LayerSelectionBound {
  LayerSelectionBound();
  ~LayerSelectionBound();

  gfx::SelectionBound::Type type;
  gfx::Point edge_start;
  gfx::Point edge_end;
  int layer_id;
  // Whether this bound is hidden (clipped out/occluded) within the painted
  // content of the layer (as opposed to being outside of the layer's bounds).
  bool hidden;

  std::string ToString() const;

  bool operator==(const LayerSelectionBound& other) const;
  bool operator!=(const LayerSelectionBound& other) const;
};

using LayerSelection = viz::Selection<LayerSelectionBound>;

}  // namespace cc

#endif  // CC_INPUT_LAYER_SELECTION_BOUND_H_
