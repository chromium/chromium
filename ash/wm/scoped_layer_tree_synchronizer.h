// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_
#define ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// Synchronizes the layer tree to ensure that render surfaces are not needed to
// draw the correct result.
class ASH_EXPORT ScopedLayerTreeSynchronizer {
 public:
  explicit ScopedLayerTreeSynchronizer(ui::Layer* root_layer);

  ScopedLayerTreeSynchronizer(const ScopedLayerTreeSynchronizer&) = delete;
  ScopedLayerTreeSynchronizer& operator=(const ScopedLayerTreeSynchronizer&) =
      delete;

  ~ScopedLayerTreeSynchronizer();

  // If needed, synchronizes the rounded corners of the subtree layers rooted
  // at root layer. This enables the rounded corners of the root layer to be
  // drawn without requiring a render surface.
  // Note that the current implementation assumes that the subtree fits within
  // the boundaries of the root layer.
  void SynchronizeRoundedCornersAvoidingRenderSurfaces();

 private:
  // Traverses through the layer subtree rooted at `layer`. When a corner of the
  // `layer` intersects or is drawn outside the curvature of the corner of the
  // `root_layer_`, the radius of that corner of the `layer` is updated.
  void SynchronizeLayerTreeRoundedCornersImpl(ui::Layer* layer);

  // The subtree that may be altered is rooted at `root_layer_`.
  const raw_ptr<ui::Layer> root_layer_;
};

}  // namespace ash

#endif  // ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_
