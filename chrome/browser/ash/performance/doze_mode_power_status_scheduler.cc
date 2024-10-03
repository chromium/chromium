// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/performance/doze_mode_power_status_scheduler.h"

#include <string_view>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/performance/pref_names.h"

namespace ash {

namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kDailyEventIntervalTimeDelta = base::Minutes(30);

constexpr char kRealPowerDurationHistogram[] =
    "Ash.DozeMode.RealPower.Duration";
constexpr char kRealBatteryDurationHistogram[] =
    "Ash.DozeMode.RealBattery.Duration";
constexpr char kSimulatedBatteryDurationHistogram[] =
    "Ash.DozeMode.SimulatedBattery.Duration";
constexpr char kRealPowerDurationPercentageHistogram[] =
    "Ash.DozeMode.RealPower.DurationPercentage";

constexpr base::TimeDelta kUserIdleTimeout = base::Seconds(30);

// For every `kMaxSimulatedBatteryStatusDuration` minutes in simulated battery
// status, force it to real power status for
// `kCompensatedRealPowerStatusDuration` minutes when charging.
constexpr base::TimeDelta kMaxSimulatedBatteryStatusDuration =
    base::Minutes(100);
constexpr base::TimeDelta kCompensatedRealPowerStatusDuration =
    base::Minutes(10);

base::TimeDelta GetAndResetPref(PrefService* pref_service,
                                std::string_view pref_name) {
  const base::TimeDelta unreported_duration =
      pref_service->GetTimeDelta(pref_name);
  pref_service->ClearPref(pref_name);
  return unreported_duration;
}

void AddToTimeDeltaPref(PrefService* pref_service,
                        std::string_view pref_name,
                        const base::TimeDelta& time_delta) {
  const base::TimeDelta unreported_duration =
      pref_service->GetTimeDelta(pref_name);
  pref_service->SetTimeDelta(pref_name, unreported_duration + time_delta);
}

}  // namespace

// This class observes notifications from a DailyEvent and forwards
// them to DozeModePowerStatusScheduler.
class DozeModePowerStatusScheduler::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(DozeModePowerStatusScheduler* scheduler)
      : scheduler_(scheduler) {}

  DailyEventObserver(const DailyEventObserver&) = delete;
  DailyEventObserver& operator=(const DailyEventObserver&) = delete;

  ~DailyEventObserver() override = default;

  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    scheduler_->ReportDailyMetrics(type);
  }

 private:
  const raw_ptr<DozeModePowerStatusScheduler> scheduler_;  // Not owned.
};

// static
void DozeModePowerStatusScheduler::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(
      registry, prefs::kDozeModePowerStatusSchedulerDailySample);

  registry->RegisterTimeDeltaPref(prefs::kDozeModeRealPowerStatusDuration,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(prefs::kDozeModeRealBatteryStatusDuration,
                                  base::TimeDelta());
  registry->RegisterTimeDeltaPref(
      prefs::kDozeModeSimulatedBatteryStatusDuration, base::TimeDelta());
}

DozeModePowerStatusScheduler::DozeModePowerStatusScheduler(
    PrefService* local_state)
    : user_active_timer_(
          FROM_HERE,
          kUserIdleTimeout,
          base::BindRepeating(
              &DozeModePowerStatusScheduler::OnUserActiveTimerTick,
              base::Unretained(this))),
      local_state_(local_state) {
  // Start to observe ARC.
  arc_session_manager_observation_.Observe(arc::ArcSessionManager::Get());
}

DozeModePowerStatusScheduler::~DozeModePowerStatusScheduler() = default;

void DozeModePowerStatusScheduler::Start() {
  last_power_status_sent_.reset();
  last_power_status_sent_time_.reset();

  // When ARC started, the following observed sources should be created already.
  arc_power_bridge_observation_.Observe(
      arc::ArcPowerBridge::GetForBrowserContext(
          arc::ArcSessionManager::Get()->profile()));
  DCHECK(arc_power_bridge_observation_.IsObserving());

  power_manager_client_observation_.Observe(
      chromeos::PowerManagerClient::Get());
  DCHECK(power_manager_client_observation_.IsObserving());
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();

  session_controller_impl_observation_.Observe(
      Shell::Get()->session_controller());
  DCHECK(session_controller_impl_observation_.IsObserving());

  video_detector_observation_.Observe(Shell::Get()->video_detector());
  DCHECK(video_detector_observation_.IsObserving());

  user_activity_observation_.Observe(ui::UserActivityDetector::Get());
  DCHECK(user_activity_observation_.IsObserving());

  // TODO(b/351086080): Move metrics recording out of scheduler.
  daily_event_.emplace(
      local_state_, prefs::kDozeModePowerStatusSchedulerDailySample,
      // Empty to skip recording the daily event type histogram.
      /* histogram_name=*/std::string());
  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  daily_event_timer_.Start(FROM_HERE, kDailyEventIntervalTimeDelta,
                           &daily_event_.value(),
                           &::metrics::DailyEvent::CheckInterval);

  simulated_battery_timer_.set_remaining_duration(
      kMaxSimulatedBatteryStatusDuration);
  force_real_power_timer_.set_remaining_duration(base::TimeDelta());
}

void DozeModePowerStatusScheduler::Stop() {
  last_power_status_sent_.reset();
  last_power_status_sent_time_.reset();

  arc_power_bridge_observation_.Reset();
  power_manager_client_observation_.Reset();
  session_controller_impl_observation_.Reset();
  video_detector_observation_.Reset();
  user_activity_observation_.Reset();

  user_active_timer_.Stop();

  simulated_battery_timer_.Stop();
  force_real_power_timer_.Stop();
  simulated_battery_timer_.set_remaining_duration(
      kMaxSimulatedBatteryStatusDuration);
  force_real_power_timer_.set_remaining_duration(base::TimeDelta());

  daily_event_timer_.Stop();
  daily_event_.reset();
}

void DozeModePowerStatusScheduler::OnArcStarted() {
  // When ARC started, initialize this class.
  Start();
}

void DozeModePowerStatusScheduler::OnArcSessionStopped(
    arc::ArcStopReason stop_reason) {
  // When ARC stopped, reset all observations (except for
  // `arc_session_manager_observation_` because it's used to observe ARC
  // restart) and stop all timers.
  Stop();
}

void DozeModePowerStatusScheduler::OnShutdown() {
  // When arc session manager shutdown, reset all observations and stop all
  // timers.
  arc_session_manager_observation_.Reset();
  Stop();
}

void DozeModePowerStatusScheduler::OnAndroidIdleStateChange(
    arc::mojom::IdleState state) {
  doze_mode_enabled_ = (state != arc::mojom::IdleState::ACTIVE);

  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::OnWillDestroyArcPowerBridge() {
  arc_power_bridge_observation_.Reset();
}

void DozeModePowerStatusScheduler::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  has_power_ =
      (proto.external_power() !=
       power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);

  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::OnLockStateChanged(bool locked) {
  screen_locked_ = locked;

  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::OnVideoStateChanged(
    ash::VideoDetector::State state) {
  video_playing_ = (state != ash::VideoDetector::State::NOT_PLAYING);

  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::OnUserActivity(const ui::Event* event) {
  user_active_timer_.Reset();

  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::OnUserActiveTimerTick() {
  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::OnSimulatedBatteryTimerTicks() {
  // For every accumulation of `kMaxSimulatedBatteryStatusDuration` of simulated
  // battery status, we add `kCompensatedRealPowerStatusDuration` of forcing
  // real power status.
  force_real_power_timer_.set_remaining_duration(
      force_real_power_timer_.get_remaining_duration() +
      kCompensatedRealPowerStatusDuration);

  // Reset to `kMaxSimulatedBatteryStatusDuration` and restart
  // `simulated_battery_timer_`.
  simulated_battery_timer_.set_remaining_duration(
      kMaxSimulatedBatteryStatusDuration);

  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::OnRealPowerTimerTicks() {
  // When `force_real_power_timer_` fires,
  // `force_real_power_timer_remaining_duration_` should be zero.
  force_real_power_timer_.set_remaining_duration(base::TimeDelta());
  MaybeUpdatePowerStatus();
}

void DozeModePowerStatusScheduler::MaybeUpdatePowerStatus() {
  const PowerStatus new_status = CalculatePowerStatus();
  if (new_status != last_power_status_sent_) {
    // Update timers to ensure enough time of real power status.
    switch (new_status) {
      case PowerStatus::kSimulatedBattery:
        simulated_battery_timer_.Start(base::BindOnce(
            &DozeModePowerStatusScheduler::OnSimulatedBatteryTimerTicks,
            base::Unretained(this)));
        if (force_real_power_timer_.IsRunning()) {
          force_real_power_timer_.Pause();
        }
        break;
      case PowerStatus::kRealPower:
        force_real_power_timer_.Start(
            base::BindOnce(&DozeModePowerStatusScheduler::OnRealPowerTimerTicks,
                           base::Unretained(this)));
        if (simulated_battery_timer_.IsRunning()) {
          simulated_battery_timer_.Pause();
        }
        break;
      case PowerStatus::kRealBattery:
        if (force_real_power_timer_.IsRunning()) {
          force_real_power_timer_.Pause();
        }
        if (simulated_battery_timer_.IsRunning()) {
          simulated_battery_timer_.Pause();
        }
        break;
    }

    SendPowerStatus(new_status);
  }
}

DozeModePowerStatusScheduler::PowerStatus
DozeModePowerStatusScheduler::CalculatePowerStatus() {
  if (!has_power_) {
    return PowerStatus::kRealBattery;
  }
  if (force_real_power_timer_.get_remaining_duration().is_positive()) {
    return PowerStatus::kRealPower;
  }
  if (doze_mode_enabled_ &&
      (user_active_timer_.IsRunning() || video_playing_) && !screen_locked_) {
    return PowerStatus::kSimulatedBattery;
  }
  return PowerStatus::kRealPower;
}

void DozeModePowerStatusScheduler::SendPowerStatus(PowerStatus status) {
  /*
  TODO(b/351086080): Add logics of sending power status to crosvm here.
  */

  const base::Time power_status_sent_time = base::Time::Now();
  if (last_power_status_sent_.has_value()) {
    // Update accumulated durations of power statuses.
    const base::TimeDelta duration =
        power_status_sent_time - last_power_status_sent_time_.value();

    switch (last_power_status_sent_.value()) {
      case PowerStatus::kRealPower:
        AddToTimeDeltaPref(local_state_,
                           prefs::kDozeModeRealPowerStatusDuration, duration);
        break;
      case PowerStatus::kRealBattery:
        AddToTimeDeltaPref(local_state_,
                           prefs::kDozeModeRealBatteryStatusDuration, duration);
        break;
      case PowerStatus::kSimulatedBattery:
        AddToTimeDeltaPref(local_state_,
                           prefs::kDozeModeSimulatedBatteryStatusDuration,
                           duration);
        break;
    }
  }

  // Update last power status and timestamp.
  last_power_status_sent_time_ = power_status_sent_time;
  last_power_status_sent_ = status;
}

void DozeModePowerStatusScheduler::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  // Do nothing on the first run.
  if (type == metrics::DailyEvent::IntervalType::FIRST_RUN) {
    return;
  }

  const base::TimeDelta real_power_status_duration =
      GetAndResetPref(local_state_, prefs::kDozeModeRealPowerStatusDuration);
  const base::TimeDelta real_battery_status_duration =
      GetAndResetPref(local_state_, prefs::kDozeModeRealBatteryStatusDuration);
  const base::TimeDelta simulated_battery_status_duration = GetAndResetPref(
      local_state_, prefs::kDozeModeSimulatedBatteryStatusDuration);
  const base::TimeDelta total_duration = simulated_battery_status_duration +
                                         real_battery_status_duration +
                                         real_power_status_duration;

  base::UmaHistogramCustomTimes(kRealPowerDurationHistogram,
                                real_power_status_duration, base::Minutes(1),
                                base::Days(1), 100);

  base::UmaHistogramCustomTimes(kRealBatteryDurationHistogram,
                                real_battery_status_duration, base::Minutes(1),
                                base::Days(1), 100);

  base::UmaHistogramCustomTimes(kSimulatedBatteryDurationHistogram,
                                simulated_battery_status_duration,
                                base::Minutes(1), base::Days(1), 100);

  if (!total_duration.is_zero()) {
    base::UmaHistogramPercentage(
        kRealPowerDurationPercentageHistogram,
        static_cast<int>(100 * (real_power_status_duration / total_duration)));
  }
}

}  // namespace ash
