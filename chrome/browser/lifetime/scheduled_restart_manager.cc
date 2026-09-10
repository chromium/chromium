// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_manager.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

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

namespace {

// Parses a "HH:MM" 24-hour time string into minutes since midnight.
// Returns std::nullopt if the string is malformed or out of range.
std::optional<uint16_t> ParseTimeToMinutes(std::string_view time_str) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      time_str, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2) {
    return std::nullopt;
  }
  int hour = 0;
  int minute = 0;
  if (!base::StringToInt(parts[0], &hour) ||
      !base::StringToInt(parts[1], &minute)) {
    return std::nullopt;
  }
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(hour * 60 + minute);
}

void RecordExecutionMetrics(
    ScheduledRestartExecutionOutcome outcome,
    const ScheduledRestartManager::BlockerSet& blockers) {
  base::UmaHistogramEnumeration("Session.ScheduledRestart.ExecutionOutcome",
                                outcome);
  for (ScheduledRestartBlocker blocker : blockers) {
    base::UmaHistogramEnumeration("Session.ScheduledRestart.BlockerEncountered",
                                  blocker);
  }
}

// Records the elapsed wall-clock duration from schedule creation to restart
// execution. Restarts executing under 10s fall into the underflow bucket (idle
// threshold is 30s in test mode and 5m in production).
void RecordTimeToUpdate(base::TimeDelta elapsed) {
  base::UmaHistogramCustomTimes(
      "Session.ScheduledRestart.TimeToUpdateAfterScheduled", elapsed,
      /*min=*/base::Seconds(10), /*max=*/base::Days(7), /*buckets=*/50);
}

}  // namespace

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

// static
ScheduledRestartManager::BlockerSet ScheduledRestartManager::GetActiveBlockers(
    const ExtendedRestartabilityState& state) {
  using SmartBlocker =
      smart_restart::ExtendedRestartabilityState::SmartRestartBlocker;

  struct BlockerMapping {
    SmartBlocker smart_blocker;
    ScheduledRestartBlocker blocker;
  };

  static constexpr BlockerMapping kBlockerMappings[] = {
      {SmartBlocker::kDownload, ScheduledRestartBlocker::kDownload},
      {SmartBlocker::kMedia, ScheduledRestartBlocker::kMedia},
      {SmartBlocker::kAudible, ScheduledRestartBlocker::kAudibleTab},
      {SmartBlocker::kCapturingVideo, ScheduledRestartBlocker::kVideoCapture},
      {SmartBlocker::kCapturingAudio, ScheduledRestartBlocker::kAudioCapture},
  };

  BlockerSet result;
  for (const auto& [smart_blocker, blocker] : kBlockerMappings) {
    if (state.blockers.Has(smart_blocker)) {
      result.Put(blocker);
    }
  }
  return result;
}

// static
std::vector<ScheduledRestartManager::TimeWindow>
ScheduledRestartManager::ParseLullWindows(std::string_view windows_str) {
  std::vector<TimeWindow> result;
  std::vector<std::string_view> windows = base::SplitStringPiece(
      windows_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (std::string_view window : windows) {
    std::vector<std::string_view> range = base::SplitStringPiece(
        window, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (range.size() != 2) {
      continue;
    }
    std::optional<uint16_t> start_min = ParseTimeToMinutes(range[0]);
    std::optional<uint16_t> end_min = ParseTimeToMinutes(range[1]);
    if (start_min && end_min) {
      result.push_back(TimeWindow{
          .start_minutes = *start_min,
          .end_minutes = *end_min,
      });
    }
  }

  return result;
}

bool ScheduledRestartManager::IsInLullWindow(base::Time time) const {
  base::Time::Exploded exploded = {};
  time.LocalExplode(&exploded);

  return std::ranges::any_of(
      lull_windows_, [current_minutes = exploded.hour * 60 + exploded.minute](
                         const TimeWindow& window) {
        if (window.start_minutes <= window.end_minutes) {
          return current_minutes >= window.start_minutes &&
                 current_minutes < window.end_minutes;
        }
        // Midnight-wrapping window (e.g. 22:00 to 06:00).
        return current_minutes >= window.start_minutes ||
               current_minutes < window.end_minutes;
      });
}

// static
base::TimeDelta ScheduledRestartManager::GetIdleThreshold() {
  const base::TimeDelta param = features::kScheduledRestartIdleThreshold.Get();
  if (param != features::kScheduledRestartIdleThreshold.default_value) {
    return param;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulateUpgrade)) {
    return base::Seconds(30);
  }
  return param;
}

// static
base::TimeDelta ScheduledRestartManager::GetFirstNudgeDelay() {
  const base::TimeDelta param =
      features::kScheduledRestartFirstNudgeDelay.Get();
  if (param != features::kScheduledRestartFirstNudgeDelay.default_value) {
    return param;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulateUpgrade)) {
    return base::Seconds(30);
  }
  return param;
}

// static
base::TimeDelta ScheduledRestartManager::GetNudgeCooldown() {
  const base::TimeDelta param = features::kScheduledRestartNudgeCooldown.Get();
  if (param != features::kScheduledRestartNudgeCooldown.default_value) {
    return param;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulateUpgrade)) {
    return base::Seconds(30);
  }
  return param;
}

bool ScheduledRestartManager::ShouldShowNudge() const {
  if (is_scheduled() || !upgrade_detector_->is_upgrade_available()) {
    return false;
  }

  const base::Time now = base::Time::Now();
  if (!is_upgrade_simulated_ && !IsInLullWindow(now)) {
    return false;
  }

  base::Time upgrade_time = upgrade_detector_->upgrade_detected_time();
  if (upgrade_time.is_null() || now - upgrade_time < GetFirstNudgeDelay()) {
    return false;
  }

  base::Time last_nudge_time = g_browser_process->local_state()->GetTime(
      prefs::kScheduledRestartLastNudgeTime);
  if (!last_nudge_time.is_null() &&
      now - last_nudge_time < GetNudgeCooldown()) {
    return false;
  }

  return true;
}

void ScheduledRestartManager::RecordNudgeShown() {
  g_browser_process->local_state()->SetTime(
      prefs::kScheduledRestartLastNudgeTime, base::Time::Now());
}

ScheduledRestartManager::ScheduledRestartManager(
    UpgradeDetector& upgrade_detector)
    : upgrade_detector_(upgrade_detector),
      is_upgrade_simulated_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulateUpgrade)),
      lull_windows_(
          ParseLullWindows(features::kScheduledRestartLullWindows.Get())),
      relaunch_callback_(
          base::BindRepeating(&chrome::RelaunchIgnoreUnloadHandlers)) {
  upgrade_detector_observation_.Observe(&upgrade_detector_.get());
  UpdateMonitoringState();
}

ScheduledRestartManager::~ScheduledRestartManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (mode_ != ScheduledRestartMode::kNone && !is_executing_restart_) {
    RecordExecutionMetrics(
        ScheduledRestartExecutionOutcome::kCanceledBeforeExecution,
        encountered_blockers_);
  }
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
  if (mode == ScheduledRestartMode::kNone) {
    if (!is_executing_restart_) {
      RecordExecutionMetrics(
          ScheduledRestartExecutionOutcome::kCanceledBeforeExecution,
          encountered_blockers_);
    }
    schedule_start_time_ = base::Time();
  } else {
    schedule_start_time_ = base::Time::Now();
  }
  encountered_blockers_.Clear();
  mode_ = mode;
  UpdateMonitoringState();
  schedule_changed_callbacks_.Notify();
}

base::CallbackListSubscription
ScheduledRestartManager::AddScheduleChangedCallback(
    ScheduleChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return schedule_changed_callbacks_.Add(std::move(callback));
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

void ScheduledRestartManager::OnIdleStateChange(
    const ui::IdlePollingService::State& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state.idle_time >= GetIdleThreshold()) {
    MaybeExecuteRestart();
  }
}

void ScheduledRestartManager::MaybeExecuteRestart() {
  if (is_executing_restart_) {
    return;
  }

  if (browser_shutdown::IsTryingToQuit()) {
    CancelSchedule();
    return;
  }

  ExtendedRestartabilityState state =
      RestartabilityMonitor::ComputeExtendedRestartabilityState();

  // Skip this attempt if conditions aren't met (e.g. active downloads, audio,
  // or video capture). The scheduled restart state is preserved so Chrome can
  // try again on subsequent idle events.
  if (!AllowsScheduledRestart(state)) {
    encountered_blockers_.PutAll(GetActiveBlockers(state));
    return;
  }

  RecordExecutionMetrics(ScheduledRestartExecutionOutcome::kSuccessOnIdle,
                         encountered_blockers_);

  if (!schedule_start_time_.is_null()) {
    base::TimeDelta elapsed = base::Time::Now() - schedule_start_time_;
    // Guard against wall-clock rollback / negative durations from manual time
    // adjustments or daylight saving time shifts.
    if (!elapsed.is_negative()) {
      RecordTimeToUpdate(elapsed);
    }
  }

  is_executing_restart_ = true;
  CancelSchedule();
  relaunch_callback_.Run();
}

}  // namespace scheduled_restart
