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

void ReportLogin(const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (data.frames_expected) {
    int smoothness = metrics_util::CalculateSmoothness(data);
    int jank = metrics_util::CalculateJank(data);

    float refresh_rate =
        Shell::GetPrimaryRootWindow()->GetHost()->compositor()->refresh_rate();
    int duration_ms = (1000.f / refresh_rate) * data.frames_expected;
    std::string suffix = GetDeviceModeSuffix();
    base::UmaHistogramPercentage("Ash.LoginAnimation.Smoothness." + suffix,
                                 smoothness);
    base::UmaHistogramPercentage("Ash.LoginAnimation.Jank." + suffix, jank);
    // TODO(crbug.com/1143898): Deprecate this metrics once the login
    // performance issue is resolved.
    base::UmaHistogramCustomTimes(
        "Ash.LoginAnimation.Duration." + suffix,
        base::TimeDelta::FromMilliseconds(duration_ms),
        base::TimeDelta::FromMilliseconds(100), base::TimeDelta::FromSeconds(5),
        50);
  } else {
    LOG(WARNING) << "Zero frames expected in login animation throughput data";
  }
}

void ReportUnlock(const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (data.frames_expected) {
    int smoothness = metrics_util::CalculateSmoothness(data);
    int jank = metrics_util::CalculateJank(data);
    std::string suffix = GetDeviceModeSuffix();
    base::UmaHistogramPercentage("Ash.UnlockAnimation.Smoothness." + suffix,
                                 smoothness);
    base::UmaHistogramPercentage("Ash.UnlockAnimation.Jank." + suffix, jank);
  } else {
    LOG(WARNING) << "Zero frames expected in Unlock animation throughput data";
  }
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
        primary_root->GetHost()->compositor(), base::BindOnce(&ReportUnlock),
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
        primary_root->GetHost()->compositor(), base::BindOnce(&ReportLogin),
        /*self_destruct=*/true);
  }
}

}  // namespace ash
