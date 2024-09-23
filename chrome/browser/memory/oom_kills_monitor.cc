// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/oom_kills_monitor.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/process/process_metrics.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace memory {

const char OOMKillsMonitor::kOOMKillsCountHistogramName[] =
    "Memory.OOMKills.Count";

const char OOMKillsMonitor::kOOMKillsDailyHistogramName[] =
    "Memory.OOMKills.Daily";

namespace {

void UmaHistogramOOMKills(int oom_kills) {
  base::UmaHistogramCustomCounts(OOMKillsMonitor::kOOMKillsCountHistogramName,
                                 oom_kills, 1, 1000, 1001);
}

// The interval at which the DailyEvent::CheckInterval function should be
// called.
constexpr base::TimeDelta kDailyEventIntervalTimeDelta = base::Minutes(30);

// The name of the histogram used to report that the OOM Kills daily event
// happened.
const char kOOMKillsDailyEventHistogramName[] =
    "Memory.OOMKills.DailyEventInterval";

}  // namespace

// This shim class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to OOMKillsMonitor.
class OOMKillsMonitor::DailyEventObserver
    : public ::metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(OOMKillsMonitor* reporter)
      : reporter_(reporter) {}

  DailyEventObserver(const DailyEventObserver&) = delete;
  DailyEventObserver& operator=(const DailyEventObserver&) = delete;

  ~DailyEventObserver() override = default;

  void OnDailyEvent(::metrics::DailyEvent::IntervalType type) override {
    reporter_->ReportDailyMetrics(type);
  }

 private:
  raw_ptr<OOMKillsMonitor> reporter_;
};

OOMKillsMonitor::OOMKillsMonitor() = default;

OOMKillsMonitor::~OOMKillsMonitor() = default;

// static
OOMKillsMonitor& OOMKillsMonitor::GetInstance() {
  static base::NoDestructor<OOMKillsMonitor> instance;
  return *instance;
}

void OOMKillsMonitor::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(::prefs::kOOMKillsDailyCount, 0);
  ::metrics::DailyEvent::RegisterPref(registry, ::prefs::kOOMKillsDailySample);
}

void OOMKillsMonitor::Initialize(PrefService* pref_service) {
  VLOG(2) << "Starting OOM kills monitor from thread "
          << base::PlatformThread::CurrentId();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (monitoring_started_) {
    NOTREACHED_IN_MIGRATION()
        << "OOM kiils monitor should only be initialized once";
    return;
  }

  monitoring_started_ = true;

  // Insert a zero kill record at the beginning for easy comparison to those
  // with non-zero kill sessions.
  UmaHistogramOOMKills(0);

  base::VmStatInfo vmstat;
  if (base::GetVmStatInfo(&vmstat)) {
    last_oom_kills_count_ = vmstat.oom_kill;
  } else {
    last_oom_kills_count_ = 0;
  }

  checking_timer_.Start(FROM_HERE, base::Minutes(1),
                        base::BindRepeating(&OOMKillsMonitor::CheckOOMKill,
                                            base::Unretained(this)));

  pref_service_ = pref_service;
  oom_kills_daily_count_ =
      pref_service_->GetInteger(::prefs::kOOMKillsDailyCount);

  daily_event_ = std::make_unique<::metrics::DailyEvent>(
      pref_service, ::prefs::kOOMKillsDailySample,
      kOOMKillsDailyEventHistogramName);
  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  daily_event_timer_.Start(FROM_HERE, kDailyEventIntervalTimeDelta,
                           daily_event_.get(),
                           &::metrics::DailyEvent::CheckInterval);
}

// Both host and guest(ARCVM) oom kills are logged to the same histogram
// Memory.OOMKills.Count.
//
// TODO(vovoy): Log ARCVM oom kills to a new histogram.
void OOMKillsMonitor::LogArcOOMKill(unsigned long current_oom_kills) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(monitoring_started_);

  unsigned long oom_kills_delta = current_oom_kills - last_arc_oom_kills_count_;
  if (oom_kills_delta == 0)
    return;

  VLOG(1) << "ARC_OOM_KILLS " << oom_kills_delta << " times";

  ReportOOMKills(oom_kills_delta);

  last_arc_oom_kills_count_ = current_oom_kills;
}

void OOMKillsMonitor::StopTimersForTesting() {
  checking_timer_.Stop();
  daily_event_timer_.Stop();
}

void OOMKillsMonitor::TriggerDailyEventForTesting() {
  ReportDailyMetrics(::metrics::DailyEvent::IntervalType::DAY_ELAPSED);
}

void OOMKillsMonitor::CheckOOMKill() {
  base::VmStatInfo vmstat;
  if (base::GetVmStatInfo(&vmstat)) {
    CheckOOMKillImpl(vmstat.oom_kill);
  }
}

void OOMKillsMonitor::CheckOOMKillImpl(unsigned long current_oom_kills) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(monitoring_started_);

  unsigned long oom_kills_delta = current_oom_kills - last_oom_kills_count_;
  if (oom_kills_delta == 0)
    return;

  VLOG(1) << "OOM_KILLS " << oom_kills_delta << " times";

  ReportOOMKills(oom_kills_delta);

  last_oom_kills_count_ = current_oom_kills;
}

void OOMKillsMonitor::ReportOOMKills(unsigned long oom_kills_delta) {
  for (size_t i = 0; i < oom_kills_delta; ++i) {
    ++oom_kills_count_;

    // Report the cumulative count of killed process. For example if there are
    // 3 processes killed, it would report 1 for the first kill, 2 for the
    // second kill, then 3 for the final kill.
    UmaHistogramOOMKills(oom_kills_count_);
  }

  oom_kills_daily_count_ += oom_kills_delta;
  pref_service_->SetInteger(::prefs::kOOMKillsDailyCount,
                            oom_kills_daily_count_);
}

void OOMKillsMonitor::ReportDailyMetrics(
    ::metrics::DailyEvent::IntervalType type) {
  if (type == ::metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    base::UmaHistogramCounts10000(kOOMKillsDailyHistogramName,
                                  oom_kills_daily_count_);
  }

  // There are 3 interval types: FIRST_RUN, DAY_ELAPSED, CLOCK_CHANGED. The
  // counter should be reset in all 3 cases.
  oom_kills_daily_count_ = 0;
  pref_service_->SetInteger(::prefs::kOOMKillsDailyCount, 0);
}

}  // namespace memory
