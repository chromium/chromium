// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_DOMAIN_DIVERSITY_REPORTER_H_
#define CHROME_BROWSER_HISTORY_DOMAIN_DIVERSITY_REPORTER_H_

#include <vector>

#include "base/scoped_observer.h"
#include "base/time/clock.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// A profile keyed service responsible for scheduling periodic tasks to report
// domain diversity metrics.
class DomainDiversityReporter : public KeyedService,
                                public history::HistoryServiceObserver {
 public:
  DomainDiversityReporter(history::HistoryService* history_service,
                          PrefService* prefs,
                          base::Clock* clock);
  ~DomainDiversityReporter() override;

  // Registers Profile preferences in |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Invokes ComputeDomainMetrics() if history backend is already loaded.
  // Otherwise, use a HistoryServiceObserver to start ComputeDomainMetrics()
  // as soon as the backend is loaded.
  void MaybeComputeDomainMetrics();

  // Computes the domain diversity metric and emits histogram through callback,
  // and schedules another domain metric computation task for 24 hours later.
  void ComputeDomainMetrics();

  // Callback to emit histograms for domain metrics.
  void ReportDomainMetrics(base::Time time_current_report_triggered,
                           history::DomainDiversityResults result);

  // HistoryServiceObserver:
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // KeyedService implementation.
  void Shutdown() override {}

 private:
  history::HistoryService* history_service_;
  PrefService* prefs_;
  base::Clock* clock_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<DomainDiversityReporter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DomainDiversityReporter);
};

#endif  // CHROME_BROWSER_HISTORY_DOMAIN_DIVERSITY_REPORTER_H_
