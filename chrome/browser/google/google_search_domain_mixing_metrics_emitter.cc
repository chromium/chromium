// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_domain_mixing_metrics_emitter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/history/core/browser/domain_mixing_metrics.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

class EmitMetricsDBTask : public history::HistoryDBTask {
 public:
  EmitMetricsDBTask(const base::Time begin_time,
                    const base::Time end_time,
                    PrefService* prefs)
      : begin_time_(begin_time), end_time_(end_time), prefs_(prefs) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    EmitDomainMixingMetrics(
        db->GetGoogleDomainVisitsFromSearchesInRange(
            begin_time_ - base::TimeDelta::FromDays(29), end_time_),
        begin_time_);
    return true;
  }

  void DoneRunOnMainThread() override {
    // Preferences must be set on the main thread.
    prefs_->SetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime,
                    end_time_);
  }

 private:
  const base::Time begin_time_;
  const base::Time end_time_;
  PrefService* const prefs_;
};

}  // namespace

const char GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime[] =
    "browser.last_google_search_domain_mixing_metrics_time";

GoogleSearchDomainMixingMetricsEmitter::GoogleSearchDomainMixingMetricsEmitter(
    PrefService* prefs,
    history::HistoryService* history_service)
    : prefs_(prefs),
      history_service_(history_service),
      ui_thread_task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})) {
  DCHECK(history_service_);
}

GoogleSearchDomainMixingMetricsEmitter::
    ~GoogleSearchDomainMixingMetricsEmitter() {}

// static
void GoogleSearchDomainMixingMetricsEmitter::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastMetricsTime, base::Time());
}

void GoogleSearchDomainMixingMetricsEmitter::Start() {
  // Schedule the task to emit domain mixing metrics the next time domain mixing
  // metrics will need to be computed.
  // If domain mixing metrics have never been computed, we start computing them
  // for active days starting now.
  base::Time last_domain_mixing_metrics_time =
      prefs_->GetTime(kLastMetricsTime);
  if (last_domain_mixing_metrics_time.is_null()) {
    // Domain mixing metrics were never computed. We start emitting metrics
    // from today 4am. This is designed so that the time windows used to compute
    // domain mixing are cut at times when the user is likely to be inactive.
    last_domain_mixing_metrics_time =
        clock_->Now().LocalMidnight() + base::TimeDelta::FromHours(4);
    prefs_->SetTime(kLastMetricsTime, last_domain_mixing_metrics_time);
  }
  ui_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GoogleSearchDomainMixingMetricsEmitter::EmitMetrics,
                     base::Unretained(this)),
      // Delay at least ten seconds to avoid delaying browser startup.
      std::max(base::TimeDelta::FromSeconds(10),
               last_domain_mixing_metrics_time + base::TimeDelta::FromDays(1) -
                   clock_->Now()));
}

void GoogleSearchDomainMixingMetricsEmitter::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void GoogleSearchDomainMixingMetricsEmitter::SetTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  timer_ = std::move(timer);
}

void GoogleSearchDomainMixingMetricsEmitter::SetUIThreadTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner) {
  ui_thread_task_runner_ = std::move(ui_thread_task_runner);
}

void GoogleSearchDomainMixingMetricsEmitter::EmitMetrics() {
  // Preferences must be accessed on the main thread so we look up here the last
  // time for which domain mixing metrics were computed.
  const base::Time begin_time = prefs_->GetTime(kLastMetricsTime);
  // We only process full days of history.
  const int days_to_compute = (clock_->Now() - begin_time).InDays();
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<EmitMetricsDBTask>(
          begin_time, begin_time + base::TimeDelta::FromDays(days_to_compute),
          prefs_),
      &task_tracker_);
  if (!timer_->IsRunning()) {
    // Run the task daily from now on.
    timer_->Start(FROM_HERE, base::TimeDelta::FromDays(1),
                  base::BindRepeating(
                      &GoogleSearchDomainMixingMetricsEmitter::EmitMetrics,
                      base::Unretained(this)));
  }
}

void GoogleSearchDomainMixingMetricsEmitter::Shutdown() {
  task_tracker_.TryCancelAll();
  timer_->Stop();
}
