// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/raster_scale_controller.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

ScopedSetRasterScale::ScopedSetRasterScale(aura::Window* window,
                                           float raster_scale)
    : window_(window), raster_scale_(raster_scale) {
  window_->AddObserver(this);
  Shell::Get()->raster_scale_controller()->PushRasterScale(window_,
                                                           raster_scale_);
}

ScopedSetRasterScale::~ScopedSetRasterScale() {
  Shutdown();
}

void ScopedSetRasterScale::UpdateScale(float raster_scale) {
  if (window_) {
    auto* controller = Shell::Get()->raster_scale_controller();
    controller->PushRasterScale(window_, raster_scale);
    controller->PopRasterScale(window_, raster_scale_);
  }
  raster_scale_ = raster_scale;
}

// static
void ScopedSetRasterScale::SetOrUpdateRasterScale(
    aura::Window* window,
    float raster_scale,
    std::unique_ptr<ScopedSetRasterScale>* p) {
  if (*p) {
    DCHECK_EQ(window, (*p)->window_);
    (*p)->UpdateScale(raster_scale);
  } else {
    *p = std::make_unique<ScopedSetRasterScale>(window, raster_scale);
  }
}

void ScopedSetRasterScale::Shutdown() {
  if (window_) {
    Shell::Get()->raster_scale_controller()->PopRasterScale(window_,
                                                            raster_scale_);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void ScopedSetRasterScale::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  Shutdown();
}

RasterScaleController::RasterScaleController() = default;
RasterScaleController::~RasterScaleController() = default;

void RasterScaleController::PushRasterScale(aura::Window* window,
                                            float raster_scale) {
  if (!windows_observation_.IsObservingSource(window)) {
    windows_observation_.AddObservation(window);
  }

  window_scales_[window].push_back(raster_scale);
  window->SetProperty(aura::client::kRasterScale,
                      GetRasterScaleForWindow(window));
}

void RasterScaleController::PopRasterScale(aura::Window* window,
                                           float raster_scale) {
  auto iter = window_scales_.find(window);

  // This may happen if the window has been destroyed.
  if (iter == window_scales_.end()) {
    return;
  }

  auto& scales = iter->second;

  DCHECK(base::Contains(scales, raster_scale));
  base::Erase(scales, raster_scale);

  if (scales.empty()) {
    window_scales_.erase(window);
    windows_observation_.RemoveObservation(window);
  }

  window->SetProperty(aura::client::kRasterScale,
                      GetRasterScaleForWindow(window));
}

float RasterScaleController::GetRasterScaleForWindow(aura::Window* window) {
  auto iter = window_scales_.find(window);
  if (iter == window_scales_.end()) {
    return 1.0f;
  }

  DCHECK(!iter->second.empty());
  float raster_scale = 0.0;
  for (auto scale : iter->second) {
    raster_scale = std::max(raster_scale, scale);
  }
  return raster_scale;
}

void RasterScaleController::OnWindowDestroying(aura::Window* window) {
  windows_observation_.RemoveObservation(window);
  window_scales_.erase(window);
}

}  // namespace ash
