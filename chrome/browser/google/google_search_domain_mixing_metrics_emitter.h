// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_DOMAIN_MIXING_METRICS_EMITTER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_DOMAIN_MIXING_METRICS_EMITTER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

// Emits domain mixing metrics based on the Google search activity of the user.
//
// The implementation schedules repeating tasks to compute the metrics when
// needed (at most once every day). The metrics are computed in the background
// and their computation is delayed at least a couple of seconds after the
// emitter is created to ensure browser startup performance is not affected.
//
// See http://goto.google.com/chrome-no-searchdomaincheck for more details on
// what domain mixing metrics are and how they are computed.
class GoogleSearchDomainMixingMetricsEmitter : public KeyedService {
 public:
  // Preference field holding the last time at which domain mixing metrics for
  // Google searches were computed, as a base::Time object. See
  // http://goto.google.com/chrome-no-searchdomaincheck for more details on what
  // domain mixing metrics are and how they are computed.
  static const char kLastMetricsTime[];

  GoogleSearchDomainMixingMetricsEmitter(
      PrefService* prefs,
      history::HistoryService* history_service);
  ~GoogleSearchDomainMixingMetricsEmitter() override;

  // Registers the preference fields used for computing Google search domain
  // mixing metrics.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Starts the emitter. The implementation will schedule a background task to
  // run the next time domain mixing metrics will need to be computed, and at
  // least a few seconds after Start() is called if metrics need to be computed
  // now. This delay is meant to ensure that browser startup performance is not
  // affected.
  void Start();

  // Overrides the default clock for testing purposes.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  // Overrides the default timer for testing purposes.
  void SetTimerForTesting(std::unique_ptr<base::RepeatingTimer> timer);

  // Overrides the UI thread task runner for testing purposes.
  void SetUIThreadTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

 private:
  // Emits metrics for active days since the last one for which metrics were
  // computed.
  void EmitMetrics();

  // KeyedService:
  void Shutdown() override;

  PrefService* const prefs_;                        // Not owned.
  history::HistoryService* const history_service_;  // Not owned.
  std::unique_ptr<base::Clock> clock_ = std::make_unique<base::DefaultClock>();
  // Timer used to compute domain mixing metrics daily if the emitter is
  // long-lived.
  std::unique_ptr<base::RepeatingTimer> timer_ =
      std::make_unique<base::RepeatingTimer>();
  base::CancelableTaskTracker task_tracker_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_DOMAIN_MIXING_METRICS_EMITTER_H_
