// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/raster_scale/raster_scale_controller.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/ranges/algorithm.h"
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
    auto* rsc = Shell::Get()->raster_scale_controller();
    if (rsc) {
      rsc->PopRasterScale(window_, raster_scale_);
    }
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void ScopedSetRasterScale::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  Shutdown();
}

ScopedPauseRasterScaleUpdates::ScopedPauseRasterScaleUpdates() {
  auto* rsc = Shell::Get()->raster_scale_controller();
  if (rsc) {
    rsc->Pause();
  }
}

ScopedPauseRasterScaleUpdates::~ScopedPauseRasterScaleUpdates() {
  auto* rsc = Shell::Get()->raster_scale_controller();
  if (rsc) {
    rsc->Unpause();
  }
}

RasterScaleController::RasterScaleController() = default;
RasterScaleController::~RasterScaleController() {
  // Reset raster scales to 1.0 on destruction.
  for (const auto& [window, _] : window_scales_) {
    window->SetProperty(aura::client::kRasterScale, 1.0f);
  }
}

float RasterScaleController::RasterScaleFromTransform(
    const gfx::Transform& transform) {
  const auto scale_2d = transform.To2dScale();
  return std::max(scale_2d.x(), scale_2d.y());
}

void RasterScaleController::PushRasterScale(aura::Window* window,
                                            float raster_scale) {
  if (!windows_observation_.IsObservingSource(window)) {
    windows_observation_.AddObservation(window);
  }

  window_scales_[window].push_back(raster_scale);
  MaybeSetRasterScale(window);
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
  auto scale_iter = base::ranges::find(scales, raster_scale);
  if (scale_iter != scales.end()) {
    scales.erase(scale_iter);
  }

  MaybeSetRasterScale(window);

  if (scales.empty()) {
    window_scales_.erase(window);

    // If we still hold a reference to this window, we need to keep observing
    // it.
    if (!pending_windows_.contains(window)) {
      windows_observation_.RemoveObservation(window);
    }
  }
}

float RasterScaleController::ComputeRasterScaleForWindow(aura::Window* window) {
  auto iter = window_scales_.find(window);
  if (iter == window_scales_.end() || iter->second.empty()) {
    return 1.0f;
  }

  float raster_scale = 0.0;
  for (auto scale : iter->second) {
    raster_scale = std::max(raster_scale, scale);
  }
  return std::max(raster_scale, kMinimumRasterScale);
}

void RasterScaleController::OnWindowDestroying(aura::Window* window) {
  windows_observation_.RemoveObservation(window);
  window_scales_.erase(window);
  pending_windows_.erase(window);
}

void RasterScaleController::MaybeSetRasterScale(aura::Window* window) {
  if (pause_count_ > 0) {
    pending_windows_.insert(window);
    return;
  }

  const float previous_scale = window->GetProperty(aura::client::kRasterScale);
  const float current_scale = ComputeRasterScaleForWindow(window);

  // Allow updating the raster scale if the relative change is at least the slop
  // proportion.
  const bool slop_condition =
      previous_scale != 0.0 &&
      std::abs((previous_scale - current_scale) / previous_scale) >=
          raster_scale_slop_proportion_;
  if (previous_scale != current_scale &&
      (slop_condition || current_scale == 1.0f)) {
    window->SetProperty(aura::client::kRasterScale, current_scale);
  }
}

void RasterScaleController::Pause() {
  pause_count_++;
}

void RasterScaleController::Unpause() {
  pause_count_--;
  DCHECK_GE(pause_count_, 0);
  if (pause_count_ == 0) {
    for (aura::Window* window : pending_windows_) {
      MaybeSetRasterScale(window);

      // If we kept observing a window since it had a pending change, we can
      // stop observing now if there is no scale set.
      if (!window_scales_.contains(window)) {
        windows_observation_.RemoveObservation(window);
      }
    }
    pending_windows_.clear();
  }
}

}  // namespace ash
