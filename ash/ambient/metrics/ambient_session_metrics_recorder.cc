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
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      session_start_time_(base::TimeTicks::Now()) {
  CHECK(delegate_);
  // Don't record this metric for `kPreview` mode.
  if (AmbientUiModel::Get()->ui_visibility() ==
      AmbientUiVisibility::kShouldShow) {
    delegate_->RecordActivation();
  }
}

AmbientSessionMetricsRecorder::~AmbientSessionMetricsRecorder() {
  auto elapsed = base::TimeTicks::Now() - session_start_time_;
  DVLOG(2) << "Exit ambient mode. Elapsed time: " << elapsed;
  delegate_->RecordEngagementTime(elapsed);

  bool ambient_ui_was_rendering = num_registered_screens_ > 0;
  if (!ambient_ui_was_rendering && elapsed >= ambient::kMetricsStartupTimeMax) {
    LOG(ERROR) << "Ambient UI completely failed to start";
    delegate_->RecordStartupTime(elapsed);
    // If `AmbientUiLauncher::Initialize()` never ran the completion callback
    // within `kMetricsStartupTimeMax`, that still counts as a failure. It
    // should be completed (either successfully or unsuccessfully by then).
    if (!session_init_status_.has_value()) {
      delegate_->RecordInitStatus(false);
    }
  }

  delegate_->RecordScreenCount(num_registered_screens_);
}

void AmbientSessionMetricsRecorder::SetInitStatus(bool init_status) {
  CHECK(!session_init_status_.has_value());
  session_init_status_ = init_status;
  delegate_->RecordInitStatus(init_status);
}

void AmbientSessionMetricsRecorder::RegisterScreen() {
  CHECK(session_init_status_.has_value() && session_init_status_.value())
      << "Ambient UI should not be rendering on screen if init failed";
  ++num_registered_screens_;
  // The very first screen registered means the ambient session has finished
  // initializing the required assets and is starting to render.
  if (num_registered_screens_ == 1 && AmbientUiModel::Get()->ui_visibility() ==
                                          AmbientUiVisibility::kShouldShow) {
    delegate_->RecordStartupTime(base::TimeTicks::Now() - session_start_time_);
  }
}

}  // namespace ash
