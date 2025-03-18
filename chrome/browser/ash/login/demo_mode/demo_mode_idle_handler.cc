// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_idle_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/experiences/idle_detector/idle_detector.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// Amount of idle time for re-launch demo mode swa with demo account login.
// TODO(crbug.com/380941267): Use a policy to control this the idle duration.
const base::TimeDelta kReLuanchDemoAppIdleDuration = base::Seconds(90);

// The list of prefs that are reset on the start of each shopper session.
const char* const kPrefsPrefixToReset[] = {
    "settings.audio", prefs::kPowerAcScreenBrightnessPercent,
    prefs::kPowerBatteryScreenBrightnessPercent};

void ResetWallpaper() {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    // This can happen in tests or shutdown.
    return;
  }

  const auto* primary_user = user_manager->GetPrimaryUser();
  WallpaperController::Get()->SetDefaultWallpaper(primary_user->GetAccountId(),
                                                  /*show_wallpaper=*/true,
                                                  base::DoNothing());
}

void ResetPrefs() {
  auto* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  auto* prefs = profile->GetPrefs();
  CHECK(prefs);

  for (auto* const pref : kPrefsPrefixToReset) {
    prefs->ClearPrefsWithPrefixSilently(pref);
  }
}

}  // namespace

DemoModeIdleHandler::DemoModeIdleHandler(DemoModeWindowCloser* window_closer)
    : window_closer_(window_closer) {
  user_activity_observer_.Observe(ui::UserActivityDetector::Get());
}

DemoModeIdleHandler::~DemoModeIdleHandler() = default;

void DemoModeIdleHandler::OnUserActivity(const ui::Event* event) {
  // We only start the `idle_detector_` timer on the first user activity. If
  // the user is already active, we don't need to do this again.
  if (is_user_active_) {
    return;
  }

  CHECK(!idle_detector_);
  is_user_active_ = true;

  // The idle detector also observes user activity and it resets its timer if it
  // is less than `kReLuanchDemoAppIdleDuration`.
  idle_detector_ = std::make_unique<IdleDetector>(
      base::BindRepeating(&DemoModeIdleHandler::OnIdle,
                          weak_ptr_factory_.GetWeakPtr()),
      /*tick_clock=*/nullptr);
  idle_detector_->Start(kReLuanchDemoAppIdleDuration);
}

void DemoModeIdleHandler::OnIdle() {
  // Report shopper session dwell time metrics.
  DemoSessionMetricsRecorder::Get()->ReportShopperSessionDwellTime();

  // Stop idle detect clock:
  idle_detector_.reset();
  is_user_active_ = false;

  window_closer_->StartClosingApps();
  ResetPrefs();

  // Explicitly call to set default wallpaper. Clear wallpaper prefs doesn't
  // change the UI.
  ResetWallpaper();

  // TODO(crbug.com/382360715): Restore network if changed by user.
}

}  // namespace ash
