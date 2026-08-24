// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_manager.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace features {

// Controls the Scheduled Restart feature on Desktop platforms.
BASE_FEATURE(kScheduledRestart, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kScheduledRestartFirstNudgeDelay{
    &kScheduledRestart, "first_nudge_delay", base::Days(14)};

const base::FeatureParam<base::TimeDelta> kScheduledRestartNudgeCooldown{
    &kScheduledRestart, "nudge_cooldown", base::Days(14)};

const base::FeatureParam<base::TimeDelta> kScheduledRestartIdleThreshold{
    &kScheduledRestart, "idle_threshold", base::Seconds(300)};

const base::FeatureParam<std::string> kScheduledRestartLullWindows{
    &kScheduledRestart, "lull_windows", "11:30-12:30,15:00-17:00"};

}  // namespace features

namespace scheduled_restart {

using ::smart_restart::ExtendedRestartabilityState;
using ::smart_restart::RestartabilityMonitor;

// static
void ScheduledRestartManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Stored timestamp of when the Scheduled Restart reminder nudge was last
  // shown to the user. Read and updated by ScheduledRestartManager in
  // downstream CLs to enforce reminder cooldowns.
  registry->RegisterTimePref(prefs::kScheduledRestartLastNudgeTime,
                             base::Time());
}

// static
bool ScheduledRestartManager::AllowsScheduledRestart(
    const ExtendedRestartabilityState& state) {
  return !state.blockers.HasAny(
      {ExtendedRestartabilityState::SmartRestartBlocker::kDownload,
       ExtendedRestartabilityState::SmartRestartBlocker::kMedia,
       ExtendedRestartabilityState::SmartRestartBlocker::kAudible,
       ExtendedRestartabilityState::SmartRestartBlocker::kCapturingVideo,
       ExtendedRestartabilityState::SmartRestartBlocker::kCapturingAudio});
}

ScheduledRestartManager::ScheduledRestartManager(
    UpgradeDetector& upgrade_detector)
    : upgrade_detector_(upgrade_detector),
      relaunch_callback_(
          base::BindRepeating(&chrome::RelaunchIgnoreUnloadHandlers)) {
  upgrade_detector_observation_.Observe(&upgrade_detector_.get());
  UpdateMonitoringState();
}

ScheduledRestartManager::~ScheduledRestartManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ScheduledRestartManager::ScheduleRestartOnIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetSchedule(ScheduledRestartMode::kOnIdle);
}

void ScheduledRestartManager::CancelSchedule() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetSchedule(ScheduledRestartMode::kNone);
}

void ScheduledRestartManager::SetSchedule(ScheduledRestartMode mode) {
  if (mode_ == mode) {
    return;
  }
  mode_ = mode;
  UpdateMonitoringState();
}

void ScheduledRestartManager::OnUpgradeRecommended() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateMonitoringState();
}

void ScheduledRestartManager::UpdateMonitoringState() {
  if (mode_ != ScheduledRestartMode::kOnIdle ||
      !upgrade_detector_->is_upgrade_available()) {
    StopIdleMonitoring();
  } else {
    StartIdleMonitoring();
  }
}

void ScheduledRestartManager::StartIdleMonitoring() {
  if (!idle_observation_.IsObserving()) {
    idle_observation_.Observe(ui::IdlePollingService::GetInstance());
  }
}

void ScheduledRestartManager::StopIdleMonitoring() {
  idle_observation_.Reset();
}

// static
base::TimeDelta ScheduledRestartManager::GetIdleThreshold() {
  // Respect explicit feature parameter overrides if configured. Otherwise,
  // speed up the threshold during manual testing with --simulate-upgrade.
  base::TimeDelta param = features::kScheduledRestartIdleThreshold.Get();
  if (param != features::kScheduledRestartIdleThreshold.default_value) {
    return param;
  }
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSimulateUpgrade)
             ? base::Seconds(30)
             : param;
}

void ScheduledRestartManager::OnIdleStateChange(
    const ui::IdlePollingService::State& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state.idle_time >= GetIdleThreshold()) {
    MaybeExecuteRestart();
  }
}

void ScheduledRestartManager::MaybeExecuteRestart() {
  if (is_executing_restart_ || browser_shutdown::IsTryingToQuit()) {
    return;
  }

  ExtendedRestartabilityState state =
      RestartabilityMonitor::ComputeExtendedRestartabilityState();

  // Skip this attempt if conditions aren't met (e.g. active downloads, audio,
  // or video capture). The scheduled restart state is preserved so Chrome can
  // try again on subsequent idle events.
  if (!AllowsScheduledRestart(state)) {
    return;
  }

  is_executing_restart_ = true;
  CancelSchedule();
  relaunch_callback_.Run();
}

}  // namespace scheduled_restart
