// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_metrics.h"

#include <string>

#include "ash/wm/overview/overview_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/screen.h"

namespace ash {

constexpr char kOverviewStartActionHistogram[] = "Ash.Overview.StartAction";
constexpr char kOverviewEndActionHistogram[] = "Ash.Overview.EndAction";

void RecordOverviewStartAction(OverviewStartAction type) {
  UMA_HISTOGRAM_ENUMERATION(kOverviewStartActionHistogram, type);
}

void RecordOverviewEndAction(OverviewEndAction type) {
  UMA_HISTOGRAM_ENUMERATION(kOverviewEndActionHistogram, type);
}

const ui::PresentationTimeRecorder::BucketParams&
GetOverviewPresentationTimeBucketParams() {
  static const ui::PresentationTimeRecorder::BucketParams kParams(
      base::Milliseconds(20), base::Seconds(30), 100);
  return kParams;
}

const char* GetOverviewEnterPresentationTimeMetricName(
    OverviewStartAction start_action) {
#define METRIC_PREFIX "Ash.Overview.Enter.PresentationTime2."
  switch (start_action) {
    case OverviewStartAction::kDevTools:
    case OverviewStartAction::kTests:
    case OverviewStartAction::kBentoBar_DEPRECATED:
      return METRIC_PREFIX "Other";
    case OverviewStartAction::kPine:
      return METRIC_PREFIX "InformedRestore";
    case OverviewStartAction::kSplitView:
    case OverviewStartAction::kAccelerator:
    case OverviewStartAction::kDragWindowFromShelf:
    case OverviewStartAction::kExitHomeLauncher:
    case OverviewStartAction::kOverviewButton:
    case OverviewStartAction::kOverviewButtonLongPress:
    case OverviewStartAction::k3FingerVerticalScroll:
    case OverviewStartAction::kWallpaper:
    case OverviewStartAction::kOverviewDeskSwitch:
    case OverviewStartAction::kDeskButton:
    case OverviewStartAction::kFasterSplitScreenSetup:
      return display::Screen::GetScreen()->InTabletMode()
                 ? METRIC_PREFIX "UserInitiatedTablet"
                 : METRIC_PREFIX "UserInitiatedClamshell";
  }
#undef METRIC_PREFIX
}

}  // namespace ash
