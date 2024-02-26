// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RASTER_SCALE_RASTER_SCALE_LAYER_OBSERVER_H_
#define ASH_WM_RASTER_SCALE_RASTER_SCALE_LAYER_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/wm/raster_scale/raster_scale_controller.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"

namespace ash {
class RasterScaleLayerObserver;
class ScopedSetRasterScale;

// Lock which will prevent `RasterScaleLayerObserver` from deleting itself
// while it is in scope. This is used to decide when to apply raster scale
// semantics, e.g. in overview mode.
class ScopedRasterScaleLayerObserverLock {
 public:
  explicit ScopedRasterScaleLayerObserverLock(
      base::WeakPtr<RasterScaleLayerObserver> observer);

  ScopedRasterScaleLayerObserverLock(ScopedRasterScaleLayerObserverLock&&);
  ScopedRasterScaleLayerObserverLock& operator=(
      ScopedRasterScaleLayerObserverLock&&);

  ScopedRasterScaleLayerObserverLock(
      const ScopedRasterScaleLayerObserverLock&) = delete;
  ScopedRasterScaleLayerObserverLock& operator=(
      const ScopedRasterScaleLayerObserverLock&) = delete;

  ~ScopedRasterScaleLayerObserverLock();

 private:
  base::WeakPtr<RasterScaleLayerObserver> observer_;
};

// This class dynamically updates raster scale based on the scale of the given
// window. The scale is observed by looking at changes in layer animations and
// window properties. This is used, for example, in overview mode to dynamically
// reduce the raster scale of lacros windows. Check the comments on member
// variables of this class for more details.
class RasterScaleLayerObserver
    : public ui::LayerAnimationObserver,
      public aura::WindowObserver,
      public aura::client::TransientWindowClientObserver,
      public ui::LayerObserver {
 public:
  explicit RasterScaleLayerObserver(aura::Window* observe_window,
                                    ui::Layer* observe_layer,
                                    aura::Window* apply_window);

  RasterScaleLayerObserver(const RasterScaleLayerObserver&) = delete;
  RasterScaleLayerObserver& operator=(const RasterScaleLayerObserver&) = delete;

  ~RasterScaleLayerObserver() override;

  ScopedRasterScaleLayerObserverLock Lock() {
    return ScopedRasterScaleLayerObserverLock(weak_ptr_factory_.GetWeakPtr());
  }

  // ui::LayerAnimationObserver
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;

  void OnLayerAnimationWillRepeat(
      ui::LayerAnimationSequence* sequence) override;

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;

  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  void OnWindowDestroying(aura::Window* window) override;

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  void OnWindowTransformed(aura::Window* window,
                           ui::PropertyChangeReason reason) override;

  void OnWindowLayerRecreated(aura::Window* window) override;

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

  // aura::client::TransientWindowClientObserver:
  void OnTransientChildWindowAdded(aura::Window* parent,
                                   aura::Window* transient_child) override;
  void OnTransientChildWindowRemoved(aura::Window* parent,
                                     aura::Window* transient_child) override;

 private:
  friend class ScopedRasterScaleLayerObserverLock;

  void SetRasterScales(float raster_scale);

  void UpdateRasterScaleFromTransform(const gfx::Transform& transform);

  void IncrementRefCount();
  void DecrementRefCount();

  //  be careful if this is not the last call in the function.
  void MaybeShutdown();

  // Window to observe window property changes on. This is
  // different to `apply_window_` to handle mirror windows. We want to observe
  // the mirrored window, but apply raster scale changes to the original window.
  raw_ptr<aura::Window> observe_window_ = nullptr;

  // Layer to observe layer animations on. This is not `observe_window_`'s layer
  // in the case of mirrored windows. In that case, it is the mirrored layer,
  // which is not the same as the layer corresponding to `observe_window_`.
  raw_ptr<ui::Layer> observe_layer_ = nullptr;

  // Window to apply raster scale changes to. This is the same as
  // `observe_window_` if there is no window mirroring occurring. This should
  // correspond to a window which can update the underlying raster scale for its
  // content.
  raw_ptr<aura::Window> apply_window_ = nullptr;

  // We need to hold onto transient windows to apply the same raster scale to
  // them as the main `apply_window_`, and to unapply them when they are done.
  // The raster scale locks for transient windows are held in `raster_scales_`.
  base::flat_set<raw_ptr<aura::Window, CtnExperimental>> transient_windows_;

  // Holds raster scale locks for windows. This will be `apply_window_` plus any
  // transient children windows.
  base::flat_map<aura::Window*, std::unique_ptr<ScopedSetRasterScale>>
      raster_scales_;

  // `RasterScaleLayerObserver` has complicated lifetime semantics. It will stay
  // alive (not be deleted) until there are no
  // `ScopedRasterScaleLayerObserverLock` locks and there are no animations
  // referencing `observe_layer_`. If neither of these conditions is true, it
  // will delete itself. The reason for the lock so we only create
  // `RasterScaleLayerObserver` when it is necessary (e.g. overview mode) to
  // avoid introducing a long tail of unknown situations to raster scale
  // updates. The reason for counting animations is that animations may continue
  // after locks are destroyed. For example, exiting overview will immediately
  // destroy locks, but we need to keep the raster scales until the animation
  // finishes.
  int animation_count_ = 0;
  int ref_count_ = 0;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};

  base::WeakPtrFactory<RasterScaleLayerObserver> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_RASTER_SCALE_RASTER_SCALE_LAYER_OBSERVER_H_
