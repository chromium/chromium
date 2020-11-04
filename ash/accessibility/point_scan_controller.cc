// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/point_scan_controller.h"

#include "ash/accessibility/layer_animation_info.h"
#include "ash/accessibility/point_scan_layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {
// Scanning time in seconds from the start of the screen (offset = 0) to the end
// of the screen (offset = bound).
constexpr float kHorizontalScanTimeSecs = 20;
constexpr float kVerticalScanTimeSecs = 20;
}  // namespace

namespace ash {

PointScanController::PointScanController() {
  horizontal_line_layer_info_.animation_rate = kHorizontalScanTimeSecs;
  vertical_line_layer_info_.animation_rate = kVerticalScanTimeSecs;
}

PointScanController::~PointScanController() = default;

void PointScanController::Start() {
  state_ = PointScanState::kHorizontalScanning;
  horizontal_line_layer_.reset(new PointScanLayer(this));
  horizontal_line_layer_info_.offset_bound =
      horizontal_line_layer_->GetBounds().width();
  horizontal_line_layer_->StartHorizontalScanning();
}

void PointScanController::Pause() {
  state_ = PointScanState::kVerticalScanning;
  horizontal_line_layer_->PauseHorizontalScanning();
  vertical_line_layer_.reset(new PointScanLayer(this));
  vertical_line_layer_info_.offset_bound =
      vertical_line_layer_->GetBounds().height();
  vertical_line_layer_->StartVerticalScanning();
}

void PointScanController::Stop() {
  state_ = PointScanState::kOff;
  vertical_line_layer_->PauseVerticalScanning();
}

base::Optional<gfx::PointF> PointScanController::OnPointSelect() {
  switch (state_) {
    case PointScanState::kHorizontalScanning:
      Pause();
      return base::nullopt;
    case PointScanState::kVerticalScanning:
      Stop();
      return gfx::PointF(horizontal_line_layer_info_.offset,
                         vertical_line_layer_info_.offset);
    case PointScanState::kOff:
      return base::nullopt;
  }
}

bool PointScanController::IsPointScanEnabled() {
  switch (state_) {
    case PointScanState::kVerticalScanning:
    case PointScanState::kHorizontalScanning:
      return true;
    case PointScanState::kOff:
      return false;
  }
}

void PointScanController::OnDeviceScaleFactorChanged() {}

void PointScanController::OnAnimationStep(base::TimeTicks timestamp) {
  AnimateLine(timestamp);
}

void PointScanController::UpdateTimeInfo(LayerAnimationInfo* animation_info,
                                         base::TimeTicks timestamp) {
  animation_info->start_time = animation_info->change_time;
  animation_info->change_time = timestamp;
}

void PointScanController::AnimateLine(base::TimeTicks timestamp) {
  if (horizontal_line_layer_->IsMoving()) {
    ComputeOffset(&horizontal_line_layer_info_, timestamp);
    horizontal_line_layer_->SetSubpixelPositionOffset(
        gfx::Vector2dF(horizontal_line_layer_info_.offset, 0.0));
    UpdateTimeInfo(&horizontal_line_layer_info_, timestamp);
  } else if (vertical_line_layer_->IsMoving()) {
    ComputeOffset(&vertical_line_layer_info_, timestamp);
    vertical_line_layer_->SetSubpixelPositionOffset(
        gfx::Vector2dF(0.0, vertical_line_layer_info_.offset));
    UpdateTimeInfo(&vertical_line_layer_info_, timestamp);
  }
}

}  // namespace ash
