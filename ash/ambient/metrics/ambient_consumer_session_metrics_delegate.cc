// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_consumer_session_metrics_delegate.h"

#include <utility>

#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace ash {

AmbientConsumerSessionMetricsDelegate::AmbientConsumerSessionMetricsDelegate(
    AmbientUiSettings ui_settings)
    : ui_settings_(std::move(ui_settings)) {}

AmbientConsumerSessionMetricsDelegate::
    ~AmbientConsumerSessionMetricsDelegate() = default;

void AmbientConsumerSessionMetricsDelegate::RecordActivation() {
  ambient::RecordAmbientModeActivation(
      /*ui_mode=*/LockScreen::HasInstance() ? AmbientUiMode::kLockScreenUi
                                            : AmbientUiMode::kInSessionUi,
      /*tablet_mode=*/Shell::Get()->IsInTabletMode());
}

void AmbientConsumerSessionMetricsDelegate::RecordInitStatus(bool success) {
  base::UmaHistogramBoolean(
      base::StrCat({"Ash.AmbientMode.Init.", ui_settings_.ToString()}),
      success);
}

void AmbientConsumerSessionMetricsDelegate::RecordStartupTime(
    base::TimeDelta startup_time) {
  ambient::RecordAmbientModeStartupTime(startup_time, ui_settings_);
}

void AmbientConsumerSessionMetricsDelegate::RecordEngagementTime(
    base::TimeDelta engagement_time) {
  ambient::RecordAmbientModeTimeElapsed(
      engagement_time, Shell::Get()->IsInTabletMode(), ui_settings_);
}

void AmbientConsumerSessionMetricsDelegate::RecordScreenCount(int num_screens) {
  base::UmaHistogramCounts100(
      base::StrCat({"Ash.AmbientMode.ScreenCount.", ui_settings_.ToString()}),
      num_screens);
}

}  // namespace ash
