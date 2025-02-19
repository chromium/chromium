// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {

constexpr char kScannerFeatureUserStateHistogramName[] =
    "Ash.ScannerFeature.UserState";

constexpr std::string_view kOnDeviceTextDetection =
    "Ash.ScannerFeature.Timer.OnDeviceTextDetection";

void RecordScannerFeatureUserState(ScannerFeatureUserState state) {
  base::UmaHistogramEnumeration(kScannerFeatureUserStateHistogramName, state);
}

void RecordOnDeviceOcrTimerCompleted(base::TimeTicks ocr_attempt_start_time) {
  base::UmaHistogramMediumTimes(
      kOnDeviceTextDetection, base::TimeTicks::Now() - ocr_attempt_start_time);
}

void RecordSunfishSessionButtonVisibilityOnLauncherShown(bool is_visible) {
  RecordScannerFeatureUserState(
      is_visible
          ? ScannerFeatureUserState::kLauncherShownWithSunfishSessionButton
          : ScannerFeatureUserState::kLauncherShownWithoutSunfishSessionButton);
}

}  // namespace ash
