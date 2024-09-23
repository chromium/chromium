// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/window_restore_metrics.h"

#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr char kHistogramTimeToAction[] = "Ash.Pine.TimeToAction";
constexpr char kHistogramSuffixListview[] = ".Listview";
constexpr char kHistogramSuffixScreenshot[] = ".Screenshot";

}  // namespace

void RecordDialogClosing(CloseDialogType type) {
  base::UmaHistogramEnumeration(kDialogClosedHistogram, type);
}

void RecordScreenshotOnShutdownStatus(ScreenshotOnShutdownStatus status) {
  base::UmaHistogramEnumeration(kScreenshotOnShutdownStatus, status);
}

void RecordScreenshotDurations(PrefService* local_state) {
  auto record_uma = [](PrefService* local_state, const std::string& name,
                       const std::string& pref_name) -> void {
    const base::TimeDelta duration = local_state->GetTimeDelta(pref_name);
    // Don't record the metric if we don't have a value.
    if (!duration.is_zero()) {
      base::UmaHistogramTimes(name, duration);
      // Reset the pref in case the next shutdown doesn't take the screenshot.
      local_state->SetTimeDelta(pref_name, base::TimeDelta());
    }
  };

  record_uma(local_state, "Ash.Pine.ScreenshotTakenDuration",
             prefs::kInformedRestoreScreenshotTakenDuration);
  record_uma(local_state, "Ash.Pine.ScreenshotEncodeAndSaveDuration",
             prefs::kInformedRestoreScreenshotEncodeAndSaveDuration);
}

void RecordDialogScreenshotVisibility(bool visible) {
  base::UmaHistogramBoolean(kDialogScreenshotVisibility, visible);
}

void RecordScreenshotDecodeDuration(base::TimeDelta duration) {
  base::UmaHistogramTimes("Ash.Pine.ScreenshotDecodeDuration", duration);
}

void RecordTimeToAction(base::TimeDelta duration, bool showing_listview) {
  const std::string histogram_name =
      std::string(kHistogramTimeToAction) + (showing_listview
                                                 ? kHistogramSuffixListview
                                                 : kHistogramSuffixScreenshot);
  base::UmaHistogramMediumTimes(histogram_name, duration);
}

void RecordOnboardingAction(bool restore) {
  base::UmaHistogramBoolean(kInformedRestoreOnboardingHistogram, restore);
}

}  // namespace ash
