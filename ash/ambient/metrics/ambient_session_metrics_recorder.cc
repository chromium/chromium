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
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"

namespace ash {

AmbientSessionMetricsRecorder::AmbientSessionMetricsRecorder(
    AmbientUiSettings ui_settings,
    const base::TickClock* tick_clock)
    : ui_settings_(std::move(ui_settings)),
      clock_(tick_clock ? tick_clock : base::DefaultTickClock::GetInstance()),
      session_start_time_(clock_->NowTicks()) {
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
  auto elapsed = clock_->NowTicks() - session_start_time_;
  DVLOG(2) << "Exit ambient mode. Elapsed time: " << elapsed;
  ambient::RecordAmbientModeTimeElapsed(elapsed, Shell::Get()->IsInTabletMode(),
                                        ui_settings_);

  bool ambient_ui_was_rendering = num_registered_screens_ > 0;
  if (!ambient_ui_was_rendering && elapsed >= ambient::kMetricsStartupTimeMax) {
    LOG(ERROR) << "Ambient UI completely failed to start";
    ambient::RecordAmbientModeStartupTime(elapsed, ui_settings_);
  }

  base::UmaHistogramCounts100(
      base::StrCat({"Ash.AmbientMode.ScreenCount.", ui_settings_.ToString()}),
      num_registered_screens_);
}

void AmbientSessionMetricsRecorder::RegisterScreen() {
  ++num_registered_screens_;
  // The very first screen registered means the ambient session has finished
  // initializing the required assets and is starting to render.
  if (num_registered_screens_ == 1 && AmbientUiModel::Get()->ui_visibility() ==
                                          AmbientUiVisibility::kShouldShow) {
    ambient::RecordAmbientModeStartupTime(
        clock_->NowTicks() - session_start_time_, ui_settings_);
  }
}

}  // namespace ash
