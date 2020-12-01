// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"

namespace {

KaleidoscopeMetricsRecorder::FirstRunProgress MojoStepToMetricsStep(
    media::mojom::KaleidoscopeFirstRunExperienceStep mojo_step) {
  switch (mojo_step) {
    case media::mojom::KaleidoscopeFirstRunExperienceStep::kCompleted:
      return KaleidoscopeMetricsRecorder::FirstRunProgress::kCompleted;
    case media::mojom::KaleidoscopeFirstRunExperienceStep::kProviderSelection:
      return KaleidoscopeMetricsRecorder::FirstRunProgress::kProviderSelection;
    case media::mojom::KaleidoscopeFirstRunExperienceStep::kMediaFeedsConsent:
      return KaleidoscopeMetricsRecorder::FirstRunProgress::kMediaFeedsConsent;
    case media::mojom::KaleidoscopeFirstRunExperienceStep::kWelcome:
      return KaleidoscopeMetricsRecorder::FirstRunProgress::kWelcome;
  }
  NOTREACHED();
}

}  // namespace

KaleidoscopeMetricsRecorder::KaleidoscopeMetricsRecorder() = default;

KaleidoscopeMetricsRecorder::~KaleidoscopeMetricsRecorder() = default;

void KaleidoscopeMetricsRecorder::OnExitPage() {
  if (first_run_experience_step_)
    RecordFirstRunProgress(MojoStepToMetricsStep(*first_run_experience_step_));
}

void KaleidoscopeMetricsRecorder::OnFirstRunExperienceStepChanged(
    media::mojom::KaleidoscopeFirstRunExperienceStep step) {
  first_run_experience_step_ = step;

  // If the first run was completed, we can go ahead and record it.
  if (first_run_experience_step_ ==
      media::mojom::KaleidoscopeFirstRunExperienceStep::kCompleted) {
    RecordFirstRunProgress(FirstRunProgress::kCompleted);
    first_run_experience_step_.reset();
  }
}

void KaleidoscopeMetricsRecorder::RecordFirstRunProgress(
    FirstRunProgress progress) {
  base::UmaHistogramEnumeration("Media.Kaleidoscope.FirstRunProgress",
                                progress);
}
