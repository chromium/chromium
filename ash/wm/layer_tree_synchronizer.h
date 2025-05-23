// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LAYER_TREE_SYNCHRONIZER_H_
#define ASH_WM_LAYER_TREE_SYNCHRONIZER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_observer.h"

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

class LayerTreeSynchronizerBase {
 public:
  explicit LayerTreeSynchronizerBase(bool restore_tree);

  LayerTreeSynchronizerBase(const LayerTreeSynchronizerBase&) = delete;
  LayerTreeSynchronizerBase& operator=(const LayerTreeSynchronizerBase&) =
      delete;

  virtual ~LayerTreeSynchronizerBase();

  // Restores the tree to its original state if `restore_tree_` is true.
  virtual void Restore() = 0;

 protected:
  // Traverses through layer subtree rooted at `layer`, adjusting(synchronizing)
  // the radius of corners of the `layer` to match the radius with
  // `reference_bounds` if any corner is drawn outside the curvature of the
  // `reference_bounds`.
  // Returns true if any of the layers of the layer tree were altered.
  // Note: `reference_bounds` are in target space of `root_layer`;
  bool SynchronizeLayerTreeRoundedCorners(ui::Layer* layer,
                                          const ui::Layer* root_layer,
                                          const gfx::RRectF& reference_bounds,
                                          bool consider_curvature);

  // Traverses through the layer subtree rooted at `layer`. Restores the radii
  // of layer if it was updated by in `SynchronizeLayerTreeRoundedCorners()`.
  void RestoreLayerTree(ui::Layer* layer);

  // Resets the cache tracking the original information about the layers
  // that are updated during `SynchronizeLayerTreeRoundedCorners()`.
  void ResetCachedLayerInfo();

 private:
  // `transform` is the relative target transform of layer to the `root_layer`.
  bool SynchronizeLayerTreeRoundedCornersImpl(
      ui::Layer* layer,
      const gfx::RRectF& reference_bounds,
      const gfx::Transform& transform,
      bool consider_curvature);

  void RestoreLayerTreeImpl(ui::Layer* layer);

  // If true, the layer tree is restored to its old state.
  const bool restore_tree_;

  // Keeps track of the original information of layers.
  base::flat_map<
      const ui::Layer*,
      std::pair<gfx::RoundedCornersF, /*is_fast_rounded_corner=*/bool>>
      original_layers_info_;
};

// Synchronizes the layer tree to specified rounded corner bounds.
class ASH_EXPORT LayerTreeSynchronizer : public LayerTreeSynchronizerBase,
                                         public ui::LayerObserver {
 public:
  explicit LayerTreeSynchronizer(bool restore_tree);

  LayerTreeSynchronizer(const LayerTreeSynchronizer&) = delete;
  LayerTreeSynchronizer& operator=(const LayerTreeSynchronizer&) = delete;

  ~LayerTreeSynchronizer() override;

  // Synchronizes the rounded corners of the subtree layers that are rooted at
  // `layer`. (layer must be a child layer of `root_layer`). See
  // `ScopedLayerTreeSynchronizerBase::SynchronizeLayerTreeRoundedCorners()`
  // for more details.
  // Note: The current implementation assumes that the subtree is contained
  // within the layer's bounds and the bounds are in the `root_layer`'s target
  // space.
  void SynchronizeRoundedCorners(ui::Layer* layer,
                                 const ui::Layer* root_layer,
                                 const gfx::RRectF& reference_bounds);

  // LayerTreeSynchronizerBase:
  void Restore() override;

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

 private:
  base::ScopedObservation<ui::Layer, ui::LayerObserver>
      altered_layer_observation_{this};
};

// Synchronizes the layer trees of a window and its transient hierarchy to given
// rounded corner bounds.
class ASH_EXPORT WindowTreeSynchronizer : public LayerTreeSynchronizerBase,
                                          public aura::WindowObserver {
 public:
  explicit WindowTreeSynchronizer(bool restore_tree);

  WindowTreeSynchronizer(const WindowTreeSynchronizer&) = delete;
  WindowTreeSynchronizer& operator=(const WindowTreeSynchronizer&) = delete;

  ~WindowTreeSynchronizer() override;

  // Synchronizes the rounded corners of layer tree for `window` and the layer
  // trees of windows is the transient hierarchy of `window`. (window must be
  //  a child of `root_window`)
  // For each window's layer tree, the synchronization is performed as described
  // in `ScopedLayerTreeSynchronizerBase::SynchronizeLayerTreeRoundedCorners()`.
  void SynchronizeRoundedCorners(aura::Window* window,
                                 const aura::Window* root_window,
                                 const gfx::RRectF& reference_bounds,
                                 bool consider_curvature,
                                 TransientTreeIgnorePredicate ignore_predicate);

  // LayerTreeSynchronizerBase:
  void Restore() override;

  // WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Observe the windows whose layer trees have been updated.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      altered_window_observations_{this};
};

}  // namespace ash

#endif  // ASH_WM_LAYER_TREE_SYNCHRONIZER_H_
