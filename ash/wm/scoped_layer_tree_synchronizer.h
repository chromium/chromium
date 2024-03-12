// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_
#define ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"

namespace gfx {
class RRectF;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// Synchronizes the layer tree to specified rounded corner bounds.
class ASH_EXPORT ScopedLayerTreeSynchronizer {
 public:
  explicit ScopedLayerTreeSynchronizer(ui::Layer* root_layer);

  ScopedLayerTreeSynchronizer(const ScopedLayerTreeSynchronizer&) = delete;
  ScopedLayerTreeSynchronizer& operator=(const ScopedLayerTreeSynchronizer&) =
      delete;

  ~ScopedLayerTreeSynchronizer();

  // Synchronizes the rounded corners of the subtree layers that are rooted at
  // `layer`. (layer must be a child layer of root_layer). If a corner of the
  // subtree's layer intersects or is drawn outside the curvature(if any) of
  // `reference_bounds', the radius of that corner is updated(synchronized) to
  // match radius of reference_bounds.
  // Note: The current implementation assumes that the subtree is contained
  // within the layer's bounds and the bounds are in the `root_layer`'s target
  // space.
  void SynchronizeRoundedCorners(ui::Layer* layer,
                                 const gfx::RRectF& reference_bounds);

 private:
  // Traverses through the layer subtree rooted at `layer`, updates the corners
  // of `layer` if conditions described in the comment for
  // `SynchronizeRoundedCorners()`.
  // Note: `reference_bounds` are in target space of `root_layer_`;
  void SynchronizeLayerTreeRoundedCorners(ui::Layer* layer,
                                          const gfx::RRectF& reference_bounds);

  // Any subtree that may be altered is rooted at `root_layer_`. All the
  // calculation done in the target space of `root_layer_`.
  raw_ptr<ui::Layer> root_layer_;
};

}  // namespace ash

#endif  // ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_
