// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"

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

AmbientSessionMetricsRecorder::AmbientSessionMetricsRecorder(
    AmbientUiSettings ui_settings)
    : ui_settings_(std::move(ui_settings)),
      session_start_time_(base::TimeTicks::Now()) {
  // Don't record this metric for `kPreview` mode.
  if (AmbientUiModel::Get()->ui_visibility() ==
      AmbientUiVisibility::kShouldShow) {
    ambient::RecordAmbientModeActivation(
        /*ui_mode=*/LockScreen::HasInstance() ? AmbientUiMode::kLockScreenUi
                                              : AmbientUiMode::kInSessionUi,
        /*tablet_mode=*/Shell::Get()->IsInTabletMode());
  }
}

AmbientSessionMetricsRecorder::~AmbientSessionMetricsRecorder() {
  auto elapsed = base::TimeTicks::Now() - session_start_time_;
  DVLOG(2) << "Exit ambient mode. Elapsed time: " << elapsed;
  ambient::RecordAmbientModeTimeElapsed(elapsed, Shell::Get()->IsInTabletMode(),
                                        ui_settings_);

  bool ambient_ui_was_rendering = num_registered_screens_ > 0;
  if (!ambient_ui_was_rendering && elapsed >= ambient::kMetricsStartupTimeMax) {
    LOG(ERROR) << "Ambient UI completely failed to start";
    ambient::RecordAmbientModeStartupTime(elapsed, ui_settings_);
    // If `AmbientUiLauncher::Initialize()` never ran the completion callback
    // within `kMetricsStartupTimeMax`, that still counts as a failure. It
    // should be completed (either successfully or unsuccessfully by then).
    if (!session_init_status_.has_value()) {
      RecordInitStatus(false);
    }
  }

  base::UmaHistogramCounts100(
      base::StrCat({"Ash.AmbientMode.ScreenCount.", ui_settings_.ToString()}),
      num_registered_screens_);
}

void AmbientSessionMetricsRecorder::SetInitStatus(bool init_status) {
  CHECK(!session_init_status_.has_value());
  session_init_status_ = init_status;
  RecordInitStatus(init_status);
}

void AmbientSessionMetricsRecorder::RegisterScreen() {
  CHECK(session_init_status_.has_value() && session_init_status_.value())
      << "Ambient UI should not be rendering on screen if init failed";
  ++num_registered_screens_;
  // The very first screen registered means the ambient session has finished
  // initializing the required assets and is starting to render.
  if (num_registered_screens_ == 1 && AmbientUiModel::Get()->ui_visibility() ==
                                          AmbientUiVisibility::kShouldShow) {
    ambient::RecordAmbientModeStartupTime(
        base::TimeTicks::Now() - session_start_time_, ui_settings_);
  }
}

void AmbientSessionMetricsRecorder::RecordInitStatus(bool init_status) {
  base::UmaHistogramBoolean(
      base::StrCat({"Ash.AmbientMode.Init.", ui_settings_.ToString()}),
      init_status);
}

}  // namespace ash
