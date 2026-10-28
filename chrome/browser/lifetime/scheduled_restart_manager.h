// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_
#define CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/power_monitor/power_observer.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "ui/base/idle/idle_polling_service.h"

class PrefRegistrySimple;
class UpgradeDetector;

namespace features {

BASE_DECLARE_FEATURE(kScheduledRestart);
BASE_DECLARE_FEATURE(kScheduledRestartAppMenuItem);

extern const base::FeatureParam<base::TimeDelta>
    kScheduledRestartFirstNudgeDelay;
extern const base::FeatureParam<base::TimeDelta> kScheduledRestartNudgeCooldown;
extern const base::FeatureParam<base::TimeDelta> kScheduledRestartIdleThreshold;
extern const base::FeatureParam<std::string> kScheduledRestartLullWindows;

}  // namespace features

namespace scheduled_restart {

// Represents the active scheduled restart mode.
enum class ScheduledRestartMode {
  kNone,
  kOnIdle,
};

// Represents the execution outcome of a scheduled restart.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ScheduledRestartExecutionOutcome)
enum class ScheduledRestartExecutionOutcome {
  kSuccessOnIdle = 0,
  kCanceledBeforeExecution = 1,
  kMaxValue = kCanceledBeforeExecution,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:ScheduledRestartExecutionOutcome)

// Represents blocker reasons encountered while waiting for an idle restart.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ScheduledRestartBlocker)
enum class ScheduledRestartBlocker {
  kDownload = 0,
  kMedia = 1,
  kAudibleTab = 2,
  kVideoCapture = 3,
  kAudioCapture = 4,
  kRecentSystemWake = 5,
  kMaxValue = kRecentSystemWake,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:ScheduledRestartBlocker)

// Manages deferred, scheduled browser restarts after Chrome updates.
//
// When a Chrome update is available, the user can schedule a restart to occur
// when the system next becomes idle. This manager observes `UpgradeDetector`
// and `ui::IdlePollingService`. The restart only takes place once gating
// conditions (such as the absence of active downloads, audio playback, or media
// capture) are satisfied.
//
// There is a single, browser-wide instance of this class whose lifetime is
// managed by `GlobalFeatures`. It can be accessed via
// `g_browser_process->GetFeatures()->scheduled_restart_manager()`.
class ScheduledRestartManager : public UpgradeObserver,
                                public ui::IdlePollingService::Observer,
                                public base::PowerSuspendObserver {
 public:
  // Wall-clock vs. base::LiveTicks skew between consecutive idle polls must
  // exceed this threshold to be treated as a system suspend. Kept above zero
  // to ignore minor NTP or manual clock adjustments while still catching
  // brief suspends. Also used as a safety margin in the user-input check,
  // because `state.idle_time` is rounded down to whole seconds.
  static constexpr base::TimeDelta kMinDetectedSuspend = base::Seconds(5);

  explicit ScheduledRestartManager(UpgradeDetector& upgrade_detector);
  ScheduledRestartManager(const ScheduledRestartManager&) = delete;
  ScheduledRestartManager& operator=(const ScheduledRestartManager&) = delete;
  ~ScheduledRestartManager() override;

  ScheduledRestartMode mode() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mode_;
  }
  bool is_scheduled() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mode_ != ScheduledRestartMode::kNone;
  }
  UpgradeDetector* upgrade_detector() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &upgrade_detector_.get();
  }

  // Schedules a restart when the browser/system next becomes idle.
  void ScheduleRestartOnIdle();

  // Cancels any active restart schedule and stops monitoring.
  void CancelSchedule();

  // Registers a callback to be notified whenever a restart schedule is set,
  // changed, or cleared (e.g., used by AppMenuIconController to update UI
  // badges).
  using ScheduleChangedCallback = base::RepeatingClosure;
  [[nodiscard]] base::CallbackListSubscription AddScheduleChangedCallback(
      ScheduleChangedCallback callback);

  // Returns true if the Scheduled Restart feature is enabled and the browser
  // is not enterprise-managed (unless an upgrade simulation switch is active).
  static bool AreScheduledRestartsEnabled();

  // Returns true if both the Scheduled Restart feature (on an unmanaged
  // browser) and the Scheduled Restart App Menu item feature are enabled.
  static bool IsAppMenuItemEnabled();

  // Returns true if a valid pending upgrade is available for scheduling a
  // restart (i.e. an upgrade is available, not an outdated install, and a
  // restart is not already scheduled).
  bool IsUpgradeAvailableForScheduling() const;

  // Evaluates whether an update reminder nudge should be presented based on
  // unpatched delay, nudge cooldown, and daily lull hours.
  bool ShouldShowNudge() const;

  // Records the current timestamp as the last nudge presentation time.
  void RecordNudgeShown();

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

  // ui::IdlePollingService::Observer:
  void OnIdleStateChange(const ui::IdlePollingService::State& state) override;

  // base::PowerSuspendObserver:
  void OnSuspend() override;
  void OnResume() override;

  // Registers Local State preferences used by ScheduledRestartManager.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  using BlockerSet =
      base::EnumSet<ScheduledRestartBlocker,
                    /*MinEnumValue=*/ScheduledRestartBlocker::kDownload,
                    /*MaxEnumValue=*/ScheduledRestartBlocker::kMaxValue>;

  // Returns true if the restart can proceed given the current restartability
  // state (e.g. no active downloads, media, audio, or video capturing).
  static bool AllowsScheduledRestart(
      const smart_restart::ExtendedRestartabilityState& state);

  // Returns the set of active scheduled restart blockers present in `state`.
  static BlockerSet GetActiveBlockers(
      const smart_restart::ExtendedRestartabilityState& state);

  // Returns the duration of user inactivity required to trigger an idle
  // restart. Defaults to the `idle_threshold` feature parameter, but is
  // overridden to 30s when `--simulate-upgrade` is active for manual testing.
  static base::TimeDelta GetIdleThreshold();

  // Returns the configured first nudge delay, or 30s during simulation.
  static base::TimeDelta GetFirstNudgeDelay();

  // Returns the configured nudge cooldown, or 30s during simulation.
  static base::TimeDelta GetNudgeCooldown();

  // Evaluates whether the given time falls within configured lull windows.
  bool IsInLullWindow(base::Time time) const;

  void set_relaunch_callback_for_testing(base::RepeatingClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    relaunch_callback_ = std::move(callback);
  }

 private:
  struct TimeWindow {
    uint16_t start_minutes = 0;  // Minutes since midnight [0, 1440).
    uint16_t end_minutes = 0;    // Minutes since midnight [0, 1440).
  };

  // Parses a comma-separated list of "HH:MM-HH:MM" lull window ranges.
  static std::vector<TimeWindow> ParseLullWindows(std::string_view windows_str);

  void SetSchedule(ScheduledRestartMode mode)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void UpdateMonitoringState() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void StartIdleMonitoring() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void StopIdleMonitoring() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void MaybeExecuteRestart() VALID_CONTEXT_REQUIRED(sequence_checker_);

  const raw_ref<UpgradeDetector> upgrade_detector_;
  const bool is_upgrade_simulated_;
  const std::vector<TimeWindow> lull_windows_;
  ScheduledRestartMode mode_ GUARDED_BY_CONTEXT(sequence_checker_) =
      ScheduledRestartMode::kNone;

  base::ScopedObservation<UpgradeDetector, UpgradeObserver>
      upgrade_detector_observation_ GUARDED_BY_CONTEXT(sequence_checker_){this};
  base::ScopedObservation<ui::IdlePollingService,
                          ui::IdlePollingService::Observer>
      idle_observation_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  base::RepeatingClosure relaunch_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingClosureList schedule_changed_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Uses base::Time instead of base::TimeTicks so that elapsed time includes
  // system suspend duration across all desktop platforms when measuring
  // TimeToUpdateAfterScheduled.
  base::Time schedule_start_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Wall-clock and awake-clock readings from the previous idle poll (seeded
  // when monitoring starts), used to detect suspend skew across polls.
  base::Time last_idle_poll_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::LiveTicks last_idle_poll_live_ticks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Awake-clock timestamp of the most recent system wake (from OnResume() or
  // suspend skew). Restarts are deferred until GetIdleThreshold() of
  // base::LiveTicks has elapsed since this point.
  base::LiveTicks last_wake_live_ticks_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Set by OnSuspend(). Cleared by OnResume(), or by OnIdleStateChange() when
  // clock skew or user input shows that the system is awake, since OnResume()
  // is best-effort and may never arrive.
  bool is_system_suspended_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  // Awake-clock timestamp of the most recent OnSuspend(), used to detect user
  // input that happened after it.
  base::LiveTicks suspend_live_ticks_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_executing_restart_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  // Tracks unique blocker types encountered while waiting for an idle restart
  // to execute. Emitted to UMA once upon schedule completion (success or
  // cancellation).
  BlockerSet encountered_blockers_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_
