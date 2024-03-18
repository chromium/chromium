// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_
#define ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class RRectF;
class RoundedCornersF;
class Transform;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class ScopedLayerTreeSynchronizerBase {
 public:
  ScopedLayerTreeSynchronizerBase(ui::Layer* root_layer, bool restore_tree);

  ScopedLayerTreeSynchronizerBase(const ScopedLayerTreeSynchronizerBase&) =
      delete;
  ScopedLayerTreeSynchronizerBase& operator=(
      const ScopedLayerTreeSynchronizerBase&) = delete;

  virtual ~ScopedLayerTreeSynchronizerBase();

  // Restores the tree to its original state if `restore_tree_` is true.
  virtual void Restore() = 0;

 protected:
  // Traverses through the layer subtree rooted at `layer`, updates the corners
  // of `layer` if conditions described in the comment for
  // `SynchronizeRoundedCorners()`.
  // Returns true if any of the layers of the layer tree were altered.
  // Note: `reference_bounds` are in target space of `root_layer_`;
  bool SynchronizeLayerTreeRoundedCorners(ui::Layer* layer,
                                          const gfx::RRectF& reference_bounds);

  // Traverses through the layer subtree rooted at `layer`. Restores the radii
  // of layer if it was updated by calling `SynchronizeRoundedCorners()`.
  void RestoreLayerTree(ui::Layer* layer);

 protected:
  ui::Layer* root_layer() { return root_layer_; }

 private:
  // `transform` is the relative target transform of layer to the `root_layer`.
  bool SynchronizeLayerTreeRoundedCornersImpl(
      ui::Layer* layer,
      const gfx::RRectF& reference_bounds,
      const gfx::Transform& transform);

  void RestoreLayerTreeImpl(ui::Layer* layer);

  // Any subtree that may be altered is rooted at `root_layer_`. All the
  // calculation done in the target space of `root_layer_`.
  raw_ptr<ui::Layer> root_layer_;

  // If true, the layer tree is restored to its old state.
  const bool restore_tree_;

  // Keeps track of the original radii of layers.
  base::flat_map<const ui::Layer*, std::pair<gfx::RoundedCornersF, bool>>
      original_layers_radii_;
};

// Synchronizes the layer tree to specified rounded corner bounds.
class ASH_EXPORT ScopedLayerTreeSynchronizer
    : public ScopedLayerTreeSynchronizerBase {
 public:
  ScopedLayerTreeSynchronizer(ui::Layer* root_layer, bool restore_tree);

  ScopedLayerTreeSynchronizer(const ScopedLayerTreeSynchronizer&) = delete;
  ScopedLayerTreeSynchronizer& operator=(const ScopedLayerTreeSynchronizer&) =
      delete;

  ~ScopedLayerTreeSynchronizer() override;

  // Synchronizes the rounded corners of the subtree layers that are rooted at
  // `layer`. (layer must be a child layer of `root_layer`). If a corner of the
  // subtree's layer intersects or is drawn outside the curvature(if any) of
  // `reference_bounds', the radius of that corner is updated(synchronized) to
  // match radius of reference_bounds.
  // Note: The current implementation assumes that the subtree is contained
  // within the layer's bounds and the bounds are in the `root_layer`'s target
  // space.
  void SynchronizeRoundedCorners(ui::Layer* layer,
                                 const gfx::RRectF& reference_bounds);

  // ScopedLayerTreeSynchronizerBase:
  void Restore() override;
};

// Synchronizes the layer trees of a window and its transient hierarchy to given
// rounded corner bounds.
class ASH_EXPORT ScopedWindowTreeSynchronizer
    : public ScopedLayerTreeSynchronizerBase,
      public aura::WindowObserver {
 public:
  ScopedWindowTreeSynchronizer(aura::Window* root_window, bool restore_tree);

  ScopedWindowTreeSynchronizer(const ScopedWindowTreeSynchronizer&) = delete;
  ScopedWindowTreeSynchronizer& operator=(const ScopedWindowTreeSynchronizer&) =
      delete;

  ~ScopedWindowTreeSynchronizer() override;

  // Synchronizes the rounded corners of layer tree for `window` and the layer
  // trees of windows is the transient hierarchy of `window`. For each window's
  // layer tree, the synchronization is performed as described for
  // `ScopedLayerTreeSynchronizer::SynchronizeRoundedCorners()`.
  void SynchronizeRoundedCorners(aura::Window* window,
                                 const gfx::RRectF& reference_bounds,
                                 TransientTreeIgnorePredicate ignore_predicate);

  // ScopedLayerTreeSynchronizerBase:
  void Restore() override;

  // WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Observe the windows whose layer trees have been updated.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      altered_window_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_SCOPED_LAYER_TREE_SYNCHRONIZER_H_
