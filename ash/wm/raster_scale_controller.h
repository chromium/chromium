// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RASTER_SCALE_CONTROLLER_H_
#define ASH_WM_RASTER_SCALE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// ScopedPauseRasterScaleUpdates pauses any updates to all windows raster scales
// until it is destructed. Once it is destructed and there are no other
// ScopedPauseRasterScaleUpdates objects, windows that still exist will have
// their raster scales updated if they have changed.
class ASH_EXPORT ScopedPauseRasterScaleUpdates {
 public:
  ScopedPauseRasterScaleUpdates();

  ScopedPauseRasterScaleUpdates(const ScopedPauseRasterScaleUpdates&) = delete;
  ScopedPauseRasterScaleUpdates& operator=(
      const ScopedPauseRasterScaleUpdates&) = delete;
  ~ScopedPauseRasterScaleUpdates();
};

// ScopedSetRasterScale keeps the raster scale property of the given window
// at or above the given value while in scope. This is necessary because,
// for example, if a window is shown both in overview mode and virtual desks
// preview, we don't want to reduce its raster scale to the tiny preview
// for the virtual desks preview. Instead, we want to use the raster scale
// for the largest preview of the window currently visible.
class ASH_EXPORT ScopedSetRasterScale : public aura::WindowObserver {
 public:
  ScopedSetRasterScale(aura::Window* window, float raster_scale);

  ScopedSetRasterScale(const ScopedSetRasterScale&) = delete;
  ScopedSetRasterScale& operator=(const ScopedSetRasterScale&) = delete;
  ~ScopedSetRasterScale() override;

  void UpdateScale(float raster_scale);

  static void SetOrUpdateRasterScale(aura::Window* window,
                                     float raster_scale,
                                     std::unique_ptr<ScopedSetRasterScale>* p);

 private:
  void Shutdown();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  raw_ptr<aura::Window> window_;
  float raster_scale_;
};

// RasterScaleController keeps track of a list of raster scales for each window.
// It is necessary to do this, because we always want to raster each window at
// the largest scale it needs to be displayed at. For example, if a window is
// displayed in overview mode and previewed in a virtual desk, both of those may
// want different scale factors. But, since the window is displayed larger in
// overview mode, we want to make sure to raster it at the higher scale to avoid
// blurriness.
class ASH_EXPORT RasterScaleController : public aura::WindowObserver {
 public:
  RasterScaleController();

  RasterScaleController(const RasterScaleController&) = delete;
  RasterScaleController& operator=(const RasterScaleController&) = delete;
  ~RasterScaleController() override;

  // Adds a raster scale to be tracked for a window. The kRasterScale property
  // for this window will be set to the largest raster scale currently active,
  // or 1.0 if there are no raster scales active.
  //
  // N.B. This relies on float equality. Currently, this is okay because all
  // usages of this are from ScopedSetRasterScale which uses the same float
  // value for push and pop. We will need to change the design if raster_scale
  // is being recomputed between the push and pop.
  void PushRasterScale(aura::Window* window, float raster_scale);

  void PopRasterScale(aura::Window* window, float raster_scale);

  float ComputeRasterScaleForWindow(aura::Window* window);

 private:
  friend class ScopedPauseRasterScaleUpdates;

  void Pause();
  void Unpause();

  void MaybeSetRasterScale(aura::Window* window);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  int pause_count_ = 0;

  base::flat_map<aura::Window*, std::vector<float>> window_scales_;

  // Holds a set of windows that have had their raster scales change while
  // RasterScaleController is paused.
  base::flat_set<aura::Window*> pending_windows_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_RASTER_SCALE_CONTROLLER_H_
