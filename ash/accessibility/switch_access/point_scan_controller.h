// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_CONTROLLER_H_
#define ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_CONTROLLER_H_

#include <optional>

#include "ash/accessibility/switch_access/point_scan_layer_animation_info.h"
#include "ash/accessibility/ui/accessibility_animation_one_shot.h"
#include "ash/accessibility/ui/accessibility_layer.h"
#include "ash/ash_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

class PointScanLayer;

// PointScanController handles drawing and animating custom lines onscreen, for
// the purposes of selecting a point onscreen without using a traditional mouse
// or keyboard. Currently used by Switch Access.
class ASH_EXPORT PointScanController : public AccessibilityLayerDelegate {
 public:
  PointScanController();
  ~PointScanController() override;

  PointScanController(const PointScanController&) = delete;
  PointScanController& operator=(const PointScanController&) = delete;

  enum class PointScanState {
    // Point scanning is not scanning.
    kOff,
    // Point scanning is currently range scanning horizontally.
    kHorizontalRangeScanning,
    // Point scanning is currently scanning horizontally.
    kHorizontalScanning,
    // Point scanning is currently range scanning vertically.
    kVerticalRangeScanning,
    // Point scanning is currently scanning vertically.
    kVerticalScanning,
  };

  // Starts point scanning, by sweeping a range across the screen and waiting
  // for user input.
  void Start();
  void StartHorizontalRangeScan();
  void StartHorizontalLineScan();
  void StartVerticalRangeScan();
  void StartVerticalLineScan();
  void Stop();
  void HideAll();
  void ResetAnimation();
  std::optional<gfx::PointF> OnPointSelect();
  bool IsPointScanEnabled();
  void SetSpeedDipsPerSecond(int speed_dips_per_second);

 private:
  // AccessibilityLayerDelegate implementation:
  void OnDeviceScaleFactorChanged() override;

  void UpdateTimeInfo(PointScanLayerAnimationInfo* animation_info,
                      base::TimeTicks timestamp);
  bool AnimateLine(base::TimeTicks timestamp);

  PointScanLayerAnimationInfo horizontal_range_layer_info_;
  std::unique_ptr<PointScanLayer> horizontal_range_layer_;
  PointScanLayerAnimationInfo horizontal_line_layer_info_;
  std::unique_ptr<PointScanLayer> horizontal_line_layer_;
  PointScanLayerAnimationInfo vertical_range_layer_info_;
  std::unique_ptr<PointScanLayer> vertical_range_layer_;
  PointScanLayerAnimationInfo vertical_line_layer_info_;
  std::unique_ptr<PointScanLayer> vertical_line_layer_;
  std::unique_ptr<AccessibilityAnimationOneShot> point_scan_animation_;

  PointScanState state_ = PointScanState::kOff;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_CONTROLLER_H_
