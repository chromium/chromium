// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/point_scan_controller.h"
#include "ash/accessibility/point_scan_layer.h"
#include "ash/accessibility/point_scan_layer_animation_info.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {
// Scanning time in seconds from the start of the screen (offset_start) to the
// end of the screen (offset_bound).
constexpr float kHorizontalScanTimeSecs = 90;
constexpr float kVerticalScanTimeSecs = 60;
constexpr float kHorizontalRangeScanTimeSecs = 30;
constexpr float kVerticalRangeScanTimeSecs = 25;
constexpr int kDefaultRangeWidthDips = 150;
constexpr float kDefaultRangeHeightDips = 120;

}  // namespace

namespace ash {

PointScanController::PointScanController() {
  horizontal_line_layer_info_.animation_rate = kHorizontalScanTimeSecs;
  horizontal_range_layer_info_.animation_rate = kHorizontalRangeScanTimeSecs;
  vertical_line_layer_info_.animation_rate = kVerticalScanTimeSecs;
  vertical_range_layer_info_.animation_rate = kVerticalRangeScanTimeSecs;
}

PointScanController::~PointScanController() = default;

void PointScanController::StartHorizontalRangeScan() {
  state_ = PointScanState::kHorizontalRangeScanning;
  horizontal_range_layer_.reset(new PointScanLayer(this));
  horizontal_range_layer_info_.offset_bound =
      horizontal_range_layer_->GetBounds().width() - kDefaultRangeWidthDips;
  horizontal_range_layer_->StartHorizontalRangeScanning();
}

void PointScanController::StartHorizontalLineScan() {
  state_ = PointScanState::kHorizontalScanning;
  horizontal_range_layer_->PauseHorizontalRangeScanning();
  horizontal_line_layer_.reset(new PointScanLayer(this));
  horizontal_line_layer_info_.offset = horizontal_range_layer_info_.offset;
  horizontal_line_layer_info_.offset_start =
      horizontal_range_layer_info_.offset;
  horizontal_line_layer_info_.offset_bound =
      horizontal_range_layer_info_.offset + kDefaultRangeWidthDips;
  horizontal_line_layer_->StartHorizontalScanning();
}

void PointScanController::StartVerticalRangeScan() {
  state_ = PointScanState::kVerticalRangeScanning;
  horizontal_line_layer_->PauseHorizontalScanning();
  horizontal_range_layer_->SetOpacity(0);
  vertical_range_layer_.reset(new PointScanLayer(this));
  vertical_range_layer_info_.offset_bound =
      vertical_range_layer_->GetBounds().height() - kDefaultRangeHeightDips;
  vertical_range_layer_->StartVerticalRangeScanning();
}

void PointScanController::StartVerticalLineScan() {
  state_ = PointScanState::kVerticalScanning;
  vertical_range_layer_->PauseVerticalRangeScanning();
  vertical_line_layer_.reset(new PointScanLayer(this));
  vertical_line_layer_info_.offset = vertical_range_layer_info_.offset;
  vertical_line_layer_info_.offset_start = vertical_range_layer_info_.offset;
  vertical_line_layer_info_.offset_bound =
      vertical_range_layer_info_.offset + kDefaultRangeHeightDips;
  vertical_line_layer_->StartVerticalScanning();
}

void PointScanController::Stop() {
  state_ = PointScanState::kOff;
  vertical_line_layer_->PauseVerticalScanning();
  vertical_range_layer_->SetOpacity(0);
}

base::Optional<gfx::PointF> PointScanController::OnPointSelect() {
  switch (state_) {
    case PointScanState::kHorizontalRangeScanning:
      StartHorizontalLineScan();
      return base::nullopt;
    case PointScanState::kHorizontalScanning:
      StartVerticalRangeScan();
      return base::nullopt;
    case PointScanState::kVerticalRangeScanning:
      StartVerticalLineScan();
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
    case PointScanState::kHorizontalRangeScanning:
    case PointScanState::kHorizontalScanning:
    case PointScanState::kVerticalRangeScanning:
    case PointScanState::kVerticalScanning:
      return true;
    case PointScanState::kOff:
      return false;
  }
}

void PointScanController::OnDeviceScaleFactorChanged() {}

void PointScanController::OnAnimationStep(base::TimeTicks timestamp) {
  AnimateLine(timestamp);
}

void PointScanController::UpdateTimeInfo(
    PointScanLayerAnimationInfo* animation_info,
    base::TimeTicks timestamp) {
  animation_info->start_time = animation_info->change_time;
  animation_info->change_time = timestamp;
}

void PointScanController::AnimateLine(base::TimeTicks timestamp) {
  if (horizontal_range_layer_->IsMoving()) {
    ComputeOffset(&horizontal_range_layer_info_, timestamp);
    horizontal_range_layer_->SetSubpixelPositionOffset(
        gfx::Vector2dF(horizontal_range_layer_info_.offset, 0.0));
    UpdateTimeInfo(&horizontal_range_layer_info_, timestamp);
  } else if (horizontal_line_layer_->IsMoving()) {
    ComputeOffset(&horizontal_line_layer_info_, timestamp);
    horizontal_line_layer_->SetSubpixelPositionOffset(
        gfx::Vector2dF(horizontal_line_layer_info_.offset, 0.0));
    UpdateTimeInfo(&horizontal_line_layer_info_, timestamp);
  } else if (vertical_range_layer_->IsMoving()) {
    ComputeOffset(&vertical_range_layer_info_, timestamp);
    vertical_range_layer_->SetSubpixelPositionOffset(
        gfx::Vector2dF(0.0, vertical_range_layer_info_.offset));
    UpdateTimeInfo(&vertical_range_layer_info_, timestamp);
  } else if (vertical_line_layer_->IsMoving()) {
    ComputeOffset(&vertical_line_layer_info_, timestamp);
    vertical_line_layer_->SetSubpixelPositionOffset(
        gfx::Vector2dF(0.0, vertical_line_layer_info_.offset));
    UpdateTimeInfo(&vertical_line_layer_info_, timestamp);
  }
}

}  // namespace ash
