// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_
#define CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
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
                                public ui::IdlePollingService::Observer {
 public:
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

  // Schedules a restart when the browser/system next becomes idle.
  void ScheduleRestartOnIdle();

  // Cancels any active restart schedule and stops monitoring.
  void CancelSchedule();

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

  // ui::IdlePollingService::Observer:
  void OnIdleStateChange(const ui::IdlePollingService::State& state) override;

  // Registers Local State preferences used by ScheduledRestartManager.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Returns true if the restart can proceed given the current restartability
  // state (e.g. no active downloads, media, audio, or video capturing).
  static bool AllowsScheduledRestart(
      const smart_restart::ExtendedRestartabilityState& state);

  // Returns the duration of user inactivity required to trigger an idle
  // restart. Defaults to the `idle_threshold` feature parameter, but is
  // overridden to 30s when `--simulate-upgrade` is active for manual testing.
  static base::TimeDelta GetIdleThreshold();

  void set_relaunch_callback_for_testing(base::RepeatingClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    relaunch_callback_ = std::move(callback);
  }

 private:
  void SetSchedule(ScheduledRestartMode mode)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void UpdateMonitoringState() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void StartIdleMonitoring() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void StopIdleMonitoring() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void MaybeExecuteRestart() VALID_CONTEXT_REQUIRED(sequence_checker_);

  const raw_ref<UpgradeDetector> upgrade_detector_;
  ScheduledRestartMode mode_ GUARDED_BY_CONTEXT(sequence_checker_) =
      ScheduledRestartMode::kNone;

  base::ScopedObservation<UpgradeDetector, UpgradeObserver>
      upgrade_detector_observation_ GUARDED_BY_CONTEXT(sequence_checker_){this};
  base::ScopedObservation<ui::IdlePollingService,
                          ui::IdlePollingService::Observer>
      idle_observation_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  base::RepeatingClosure relaunch_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_executing_restart_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_LIFETIME_SCHEDULED_RESTART_MANAGER_H_
