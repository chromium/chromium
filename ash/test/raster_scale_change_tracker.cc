// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/raster_scale_change_tracker.h"

#include "ash/public/cpp/window_properties.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

RasterScaleChangeTracker::RasterScaleChangeTracker(aura::Window* window)
    : window_(window) {
  window->AddObserver(this);
}

RasterScaleChangeTracker::~RasterScaleChangeTracker() {
  Shutdown();
}

void RasterScaleChangeTracker::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old_value) {
  if (key == aura::client::kRasterScale) {
    float raster_scale = window->GetProperty(aura::client::kRasterScale);
    raster_scales_.push_back(raster_scale);
  }
}

void RasterScaleChangeTracker::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  Shutdown();
}

std::vector<float> RasterScaleChangeTracker::TakeRasterScaleChanges() {
  auto scales = raster_scales_;
  raster_scales_.clear();
  return scales;
}

void RasterScaleChangeTracker::Shutdown() {
  if (window_) {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

}  // namespace ash
