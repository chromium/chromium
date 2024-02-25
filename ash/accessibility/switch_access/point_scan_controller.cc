// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/switch_access/point_scan_controller.h"

#include <memory>

#include "ash/accessibility/switch_access/point_scan_layer.h"
#include "ash/accessibility/switch_access/point_scan_layer_animation_info.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

constexpr int kDefaultRangeWidthDips = 150;
constexpr int kDefaultRangeHeightDips = 120;
constexpr float kLineScanSlowDownFactor = 0.5f;

}  // namespace

namespace ash {

PointScanController::PointScanController() = default;

PointScanController::~PointScanController() {
  HideAll();
}

void PointScanController::Start() {
  HideAll();
  ResetAnimation();
  StartHorizontalRangeScan();
  point_scan_animation_ = std::make_unique<AccessibilityAnimationOneShot>(
      gfx::Rect(0, 0, 0, 0),
      base::BindRepeating(&PointScanController::AnimateLine,
                          base::Unretained(this)));
}

void PointScanController::StartHorizontalRangeScan() {
  state_ = PointScanState::kHorizontalRangeScanning;
  horizontal_range_layer_ = std::make_unique<PointScanLayer>(
      this, PointScanLayer::Orientation::HORIZONTAL,
      PointScanLayer::Type::RANGE);
  gfx::Rect layer_bounds = horizontal_range_layer_->bounds();
  horizontal_range_layer_info_.offset = layer_bounds.x();
  horizontal_range_layer_info_.offset_start = layer_bounds.x();
  horizontal_range_layer_info_.offset_bound =
      layer_bounds.right() - kDefaultRangeWidthDips;
  horizontal_range_layer_->Start();
}

void PointScanController::StartHorizontalLineScan() {
  state_ = PointScanState::kHorizontalScanning;
  horizontal_range_layer_->Pause();
  horizontal_line_layer_ = std::make_unique<PointScanLayer>(
      this, PointScanLayer::Orientation::HORIZONTAL,
      PointScanLayer::Type::LINE);
  horizontal_line_layer_info_.offset = horizontal_range_layer_info_.offset;
  horizontal_line_layer_info_.offset_start =
      horizontal_range_layer_info_.offset;
  horizontal_line_layer_info_.offset_bound =
      horizontal_range_layer_info_.offset + kDefaultRangeWidthDips;
  horizontal_line_layer_->Start();
}

void PointScanController::StartVerticalRangeScan() {
  state_ = PointScanState::kVerticalRangeScanning;
  horizontal_line_layer_->Pause();
  horizontal_range_layer_->SetOpacity(0);
  vertical_range_layer_ = std::make_unique<PointScanLayer>(
      this, PointScanLayer::Orientation::VERTICAL, PointScanLayer::Type::RANGE);
  gfx::Rect layer_bounds = vertical_range_layer_->bounds();
  vertical_range_layer_info_.offset = layer_bounds.y();
  vertical_range_layer_info_.offset = layer_bounds.y();
  vertical_range_layer_info_.offset_bound =
      layer_bounds.bottom() - kDefaultRangeHeightDips;
  vertical_range_layer_->Start();
}

void PointScanController::StartVerticalLineScan() {
  state_ = PointScanState::kVerticalScanning;
  vertical_range_layer_->Pause();
  vertical_line_layer_ = std::make_unique<PointScanLayer>(
      this, PointScanLayer::Orientation::VERTICAL, PointScanLayer::Type::LINE);
  vertical_line_layer_info_.offset = vertical_range_layer_info_.offset;
  vertical_line_layer_info_.offset_start = vertical_range_layer_info_.offset;
  vertical_line_layer_info_.offset_bound =
      vertical_range_layer_info_.offset + kDefaultRangeHeightDips;
  vertical_line_layer_->Start();
}

void PointScanController::Stop() {
  state_ = PointScanState::kOff;
  vertical_line_layer_->Pause();
  vertical_range_layer_->SetOpacity(0);
  point_scan_animation_.reset();
}

void PointScanController::HideAll() {
  if (horizontal_range_layer_) {
    horizontal_range_layer_->Pause();
    horizontal_range_layer_->SetOpacity(0);
  }
  if (horizontal_line_layer_) {
    horizontal_line_layer_->Pause();
    horizontal_line_layer_->SetOpacity(0);
  }
  if (vertical_range_layer_) {
    vertical_range_layer_->Pause();
    vertical_range_layer_->SetOpacity(0);
  }
  if (vertical_line_layer_) {
    vertical_line_layer_->Pause();
    vertical_line_layer_->SetOpacity(0);
  }
}

void PointScanController::ResetAnimation() {
  horizontal_range_layer_info_.Clear();
  if (horizontal_range_layer_)
    horizontal_range_layer_->SetSubpixelPositionOffset(gfx::Vector2dF(0, 0));

  horizontal_line_layer_info_.Clear();
  if (horizontal_line_layer_)
    horizontal_line_layer_->SetSubpixelPositionOffset(gfx::Vector2dF(0, 0));

  vertical_range_layer_info_.Clear();
  if (vertical_range_layer_)
    vertical_range_layer_->SetSubpixelPositionOffset(gfx::Vector2dF(0, 0));

  vertical_line_layer_info_.Clear();
  if (vertical_line_layer_)
    vertical_line_layer_->SetSubpixelPositionOffset(gfx::Vector2dF(0, 0));
}

std::optional<gfx::PointF> PointScanController::OnPointSelect() {
  switch (state_) {
    case PointScanState::kHorizontalRangeScanning:
      StartHorizontalLineScan();
      return std::nullopt;
    case PointScanState::kHorizontalScanning:
      StartVerticalRangeScan();
      return std::nullopt;
    case PointScanState::kVerticalRangeScanning:
      StartVerticalLineScan();
      return std::nullopt;
    case PointScanState::kVerticalScanning:
      Stop();
      return gfx::PointF(horizontal_line_layer_info_.offset,
                         vertical_line_layer_info_.offset);
    case PointScanState::kOff:
      return std::nullopt;
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

void PointScanController::SetSpeedDipsPerSecond(int speed_dips_per_second) {
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  float width = display_bounds.width();
  float height = display_bounds.height();
  horizontal_range_layer_info_.animation_rate = width / speed_dips_per_second;
  horizontal_line_layer_info_.animation_rate =
      width / (speed_dips_per_second * kLineScanSlowDownFactor);
  vertical_range_layer_info_.animation_rate = height / speed_dips_per_second;
  vertical_line_layer_info_.animation_rate =
      height / (speed_dips_per_second * kLineScanSlowDownFactor);
}

void PointScanController::OnDeviceScaleFactorChanged() {}

void PointScanController::UpdateTimeInfo(
    PointScanLayerAnimationInfo* animation_info,
    base::TimeTicks timestamp) {
  animation_info->start_time = animation_info->change_time;
  animation_info->change_time = timestamp;
}

bool PointScanController::AnimateLine(base::TimeTicks timestamp) {
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

  return false;
}

}  // namespace ash
