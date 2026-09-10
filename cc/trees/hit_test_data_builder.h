// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_HIT_TEST_DATA_BUILDER_H_
#define CC_TREES_HIT_TEST_DATA_BUILDER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "cc/base/region.h"

namespace viz {
struct HitTestRegionList;
}

namespace cc {

class LayerImpl;
class LayerTreeImpl;
class SurfaceLayerImpl;

class HitTestDataBuilder {
 public:
  explicit HitTestDataBuilder(const LayerTreeImpl& active_tree);

  std::optional<viz::HitTestRegionList> Build() &&;

 private:
  // Determines whether `layer` should emit hit test data, indicated by a
  // non-null return value. Also tracks non-emitted layers that may obscure
  // surfaces encountered later.
  const SurfaceLayerImpl* EvaluateLayerAndTrackOverlap(const LayerImpl* layer);

  void TrackHitTestableNonSurfaceLayer(const LayerImpl* layer);
  void TrackNonEmittedSurface(const SurfaceLayerImpl* surface_layer);
  bool IsSurfaceOverlapped(const SurfaceLayerImpl* surface_layer) const;
  bool ShouldAssumeOverlap() const;

  const raw_ref<const LayerTreeImpl> active_tree_;

  // TODO(sunxd): Submit all overlapping layer bounds as hit test regions. Also,
  // investigate if we can use visible layer rect as overlapping regions.
  Region overlapping_region_;
  size_t num_hit_testable_non_surface_layers_ = 0;
};

}  // namespace cc

#endif  // CC_TREES_HIT_TEST_DATA_BUILDER_H_
