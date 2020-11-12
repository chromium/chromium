// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_metrics.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {

constexpr char kCaptureRegionAdjustmentHistogramName[] =
    "Ash.CaptureModeController.CaptureRegionAdjusted";
constexpr char kBarButtonHistogramName[] =
    "Ash.CaptureModeController.BarButtons";
constexpr char kEntryHistogramName[] = "Ash.CaptureModeController.EntryPoint";
constexpr char kRecordTimeHistogramName[] =
    "Ash.CaptureModeController.ScreenRecordingLength";
constexpr char kSwitchesFromInitialModeHistogramName[] =
    "Ash.CaptureModeController.SwitchesFromInitialCaptureMode";

// Appends the proper suffix to |prefix| based on whether the user is in tablet
// mode or not.
std::string GetCaptureModeHistogramName(std::string prefix) {
  prefix.append(Shell::Get()->IsInTabletMode() ? ".TabletMode"
                                               : ".ClamshellMode");
  return prefix;
}

}  // namespace

void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kBarButtonHistogramName), button_type);
}

void RecordCaptureModeEntryType(CaptureModeEntryType entry_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kEntryHistogramName), entry_type);
}

void RecordNumberOfCaptureRegionAdjustments(int num_adjustments) {
  base::UmaHistogramCounts100(
      GetCaptureModeHistogramName(kCaptureRegionAdjustmentHistogramName),
      num_adjustments);
}

void RecordCaptureModeRecordTime(int64_t length_in_seconds) {
  // Use custom counts macro instead of custom times so we can record in
  // seconds instead of milliseconds. The max bucket is 3 hours.
  base::UmaHistogramCustomCounts(
      kRecordTimeHistogramName, length_in_seconds, /*min=*/1,
      /*max=*/base::TimeDelta::FromHours(3).InSeconds(),
      /*bucket_count=*/50);
}

void RecordCaptureModeSwitchesFromInitialMode(bool switched) {
  base::UmaHistogramBoolean(kSwitchesFromInitialModeHistogramName, switched);
}

}  // namespace ash
