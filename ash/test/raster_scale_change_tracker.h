// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_RASTER_SCALE_CHANGE_TRACKER_H_
#define ASH_TEST_RASTER_SCALE_CHANGE_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

#include <vector>

namespace ash {

class RasterScaleChangeTracker : public aura::WindowObserver {
 public:
  explicit RasterScaleChangeTracker(aura::Window* window);

  ~RasterScaleChangeTracker() override;

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old_value) override;

  void OnWindowDestroying(aura::Window* window) override;

  std::vector<float> TakeRasterScaleChanges();

 private:
  raw_ptr<aura::Window> window_;
  std::vector<float> raster_scales_;

  void Shutdown();
};

}  // namespace ash

#endif  // ASH_TEST_RASTER_SCALE_CHANGE_TRACKER_H_
