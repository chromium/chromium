// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_unlock_throughput_recorder.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/metrics/login_event_recorder.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/total_animation_throughput_reporter.h"

namespace ash {
namespace {

std::string GetDeviceModeSuffix() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode()
             ? "TabletMode"
             : "ClamshellMode";
}

void RecordMetrics(const base::TimeTicks& start,
                   const cc::FrameSequenceMetrics::CustomReportData& data,
                   const char* smoothness_name,
                   const char* jank_name,
                   const char* duration_name) {
  DCHECK(data.frames_expected);

  // Report could happen during Shell shutdown. Early out in that case.
  if (!Shell::HasInstance() || !Shell::Get()->tablet_mode_controller())
    return;

  int duration_ms = (base::TimeTicks::Now() - start).InMilliseconds();
  int smoothness, jank;
  smoothness = metrics_util::CalculateSmoothness(data);
  jank = metrics_util::CalculateJank(data);

  std::string suffix = GetDeviceModeSuffix();
  base::UmaHistogramPercentage(smoothness_name + suffix, smoothness);
  base::UmaHistogramPercentage(jank_name + suffix, jank);
  // TODO(crbug.com/1143898): Deprecate this metrics once the login/unlock
  // performance issue is resolved.
  base::UmaHistogramCustomTimes(duration_name + suffix,
                                base::Milliseconds(duration_ms),
                                base::Milliseconds(100), base::Seconds(5), 50);
}

void ReportLogin(base::TimeTicks start,
                 const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!data.frames_expected) {
    LOG(WARNING) << "Zero frames expected in login animation throughput data";
    return;
  }
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(
      "LoginAnimationEnd",
      /*send_to_uma=*/false,
      /*write_to_file=*/false);
  chromeos::LoginEventRecorder::Get()->RunScheduledWriteLoginTimes();
  RecordMetrics(start, data, "Ash.LoginAnimation.Smoothness.",
                "Ash.LoginAnimation.Jank.", "Ash.LoginAnimation.Duration.");
}

void ReportUnlock(base::TimeTicks start,
                  const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!data.frames_expected) {
    LOG(WARNING) << "Zero frames expected in unlock animation throughput data";
    return;
  }
  RecordMetrics(start, data, "Ash.UnlockAnimation.Smoothness.",
                "Ash.UnlockAnimation.Jank.", "Ash.UnlockAnimation.Duration.");
}

}  // namespace

LoginUnlockThroughputRecorder::LoginUnlockThroughputRecorder() {
  Shell::Get()->session_controller()->AddObserver(this);
  chromeos::LoginState::Get()->AddObserver(this);
}

LoginUnlockThroughputRecorder::~LoginUnlockThroughputRecorder() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  chromeos::LoginState::Get()->RemoveObserver(this);
}

void LoginUnlockThroughputRecorder::OnLockStateChanged(bool locked) {
  auto logged_in_user = chromeos::LoginState::Get()->GetLoggedInUserType();

  if (!locked &&
      (logged_in_user == chromeos::LoginState::LOGGED_IN_USER_OWNER ||
       logged_in_user == chromeos::LoginState::LOGGED_IN_USER_REGULAR)) {
    auto* primary_root = Shell::GetPrimaryRootWindow();
    new ui::TotalAnimationThroughputReporter(
        primary_root->GetHost()->compositor(),
        base::BindOnce(&ReportUnlock, base::TimeTicks::Now()),
        /*self_destruct=*/true);
  }
}

void LoginUnlockThroughputRecorder::LoggedInStateChanged() {
  auto* login_state = chromeos::LoginState::Get();
  auto logged_in_user = login_state->GetLoggedInUserType();
  if (login_state->IsUserLoggedIn() &&
      (logged_in_user == chromeos::LoginState::LOGGED_IN_USER_OWNER ||
       logged_in_user == chromeos::LoginState::LOGGED_IN_USER_REGULAR)) {
    auto* primary_root = Shell::GetPrimaryRootWindow();
    new ui::TotalAnimationThroughputReporter(
        primary_root->GetHost()->compositor(),
        base::BindOnce(&ReportLogin, base::TimeTicks::Now()),
        /*self_destruct=*/true);
  }
}

}  // namespace ash
