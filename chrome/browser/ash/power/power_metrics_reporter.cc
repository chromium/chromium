// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/power_metrics_reporter.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal = base::Seconds(60);

// Information about a daily count that should be tracked and reported.
struct DailyCountInfo {
  // Name of the local store pref backing the count across Chrome restarts.
  const char* pref_name;
  // Histogram metric name used to report the count.
  const char* metric_name;
};

// Registry of all daily counts.
const DailyCountInfo kDailyCounts[] = {
    {
        prefs::kPowerMetricsIdleScreenDimCount,
        PowerMetricsReporter::kIdleScreenDimCountName,
    },
    {
        prefs::kPowerMetricsIdleScreenOffCount,
        PowerMetricsReporter::kIdleScreenOffCountName,
    },
    {
        prefs::kPowerMetricsIdleSuspendCount,
        PowerMetricsReporter::kIdleSuspendCountName,
    },
    {
        prefs::kPowerMetricsLidClosedSuspendCount,
        PowerMetricsReporter::kLidClosedSuspendCountName,
    }};

}  // namespace

// This shim class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to PowerMetricsReporter.
class PowerMetricsReporter::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(PowerMetricsReporter* reporter)
      : reporter_(reporter) {}

  DailyEventObserver(const DailyEventObserver&) = delete;
  DailyEventObserver& operator=(const DailyEventObserver&) = delete;

  ~DailyEventObserver() override = default;

  // metrics::DailyEvent::Observer:
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    reporter_->ReportDailyMetrics(type);
  }

 private:
  raw_ptr<PowerMetricsReporter, ExperimentalAsh> reporter_;  // Not owned.
};

const char PowerMetricsReporter::kDailyEventIntervalName[] =
    "Power.MetricsDailyEventInterval";
const char PowerMetricsReporter::kIdleScreenDimCountName[] =
    "Power.IdleScreenDimCountDaily";
const char PowerMetricsReporter::kIdleScreenOffCountName[] =
    "Power.IdleScreenOffCountDaily";
const char PowerMetricsReporter::kIdleSuspendCountName[] =
    "Power.IdleSuspendCountDaily";
const char PowerMetricsReporter::kLidClosedSuspendCountName[] =
    "Power.LidClosedSuspendCountDaily";

// static
void PowerMetricsReporter::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(registry, prefs::kPowerMetricsDailySample);
  for (size_t i = 0; i < std::size(kDailyCounts); ++i)
    registry->RegisterIntegerPref(kDailyCounts[i].pref_name, 0);
}

PowerMetricsReporter::PowerMetricsReporter(
    chromeos::PowerManagerClient* power_manager_client,
    PrefService* local_state_pref_service)
    : power_manager_client_(power_manager_client),
      pref_service_(local_state_pref_service),
      daily_event_(
          std::make_unique<metrics::DailyEvent>(pref_service_,
                                                prefs::kPowerMetricsDailySample,
                                                kDailyEventIntervalName)) {
  for (size_t i = 0; i < std::size(kDailyCounts); ++i) {
    daily_counts_[kDailyCounts[i].pref_name] =
        pref_service_->GetInteger(kDailyCounts[i].pref_name);
  }
  power_manager_client_->AddObserver(this);

  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  timer_.Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
               &metrics::DailyEvent::CheckInterval);
}

PowerMetricsReporter::~PowerMetricsReporter() {
  power_manager_client_->RemoveObserver(this);
}

void PowerMetricsReporter::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& state) {
  if (state.dimmed() && !old_screen_idle_state_.dimmed())
    AddToCount(prefs::kPowerMetricsIdleScreenDimCount, 1);
  if (state.off() && !old_screen_idle_state_.off())
    AddToCount(prefs::kPowerMetricsIdleScreenOffCount, 1);

  old_screen_idle_state_ = state;
}

void PowerMetricsReporter::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  switch (reason) {
    case power_manager::SuspendImminent_Reason_IDLE:
      AddToCount(prefs::kPowerMetricsIdleSuspendCount, 1);
      break;
    case power_manager::SuspendImminent_Reason_LID_CLOSED:
      AddToCount(prefs::kPowerMetricsLidClosedSuspendCount, 1);
      break;
    case power_manager::SuspendImminent_Reason_OTHER:
      break;
  }
}

void PowerMetricsReporter::SuspendDone(base::TimeDelta duration) {
  daily_event_->CheckInterval();
}

void PowerMetricsReporter::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  // Don't send metrics on first run or if the clock is changed.
  if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    for (size_t i = 0; i < std::size(kDailyCounts); ++i) {
      base::UmaHistogramCounts100(kDailyCounts[i].metric_name,
                                  daily_counts_[kDailyCounts[i].pref_name]);
    }
  }

  // Reset all counts now that they've been reported.
  for (size_t i = 0; i < std::size(kDailyCounts); ++i) {
    const char* pref_name = kDailyCounts[i].pref_name;
    AddToCount(pref_name, -1 * daily_counts_[pref_name]);
  }
}

void PowerMetricsReporter::AddToCount(const std::string& pref_name, int num) {
  DCHECK(daily_counts_.count(pref_name));
  daily_counts_[pref_name] += num;
  pref_service_->SetInteger(pref_name, daily_counts_[pref_name]);
}

}  // namespace ash
