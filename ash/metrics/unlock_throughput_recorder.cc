// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/unlock_throughput_recorder.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/total_animation_throughput_reporter.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

std::string GetDeviceModeSuffix() {
  return display::Screen::GetScreen()->InTabletMode() ? "TabletMode"
                                                      : "ClamshellMode";
}

void ReportUnlock(const cc::FrameSequenceMetrics::CustomReportData& data,
                  base::TimeTicks first_animation_started_at,
                  base::TimeTicks animation_finished_at) {
  if (!data.frames_expected_v3) {
    LOG(WARNING) << "Zero frames expected in unlock animation throughput data";
    return;
  }

  // Report could happen during Shell shutdown. Early out in that case.
  if (!Shell::HasInstance() || !Shell::Get()->tablet_mode_controller()) {
    return;
  }

  const int smoothness = metrics_util::CalculateSmoothnessV3(data);

  constexpr char smoothness_name[] = "Ash.UnlockAnimation.Smoothness.";
  const std::string suffix = GetDeviceModeSuffix();
  base::UmaHistogramPercentage(smoothness_name + suffix, smoothness);
}

}  // namespace

UnlockThroughputRecorder::UnlockThroughputRecorder() {
  Shell::Get()->session_controller()->AddObserver(this);
}

UnlockThroughputRecorder::~UnlockThroughputRecorder() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void UnlockThroughputRecorder::OnLockStateChanged(bool locked) {
  auto logged_in_user = LoginState::Get()->GetLoggedInUserType();
  if (!locked && logged_in_user == LoginState::LOGGED_IN_USER_REGULAR) {
    auto* primary_root = Shell::GetPrimaryRootWindow();
    new ui::TotalAnimationThroughputReporter(
        primary_root->GetHost()->compositor(), base::BindOnce(&ReportUnlock),
        /*should_delete=*/true);
  }
}

}  // namespace ash
