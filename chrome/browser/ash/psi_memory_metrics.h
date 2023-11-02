// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PSI_MEMORY_METRICS_H_
#define CHROME_BROWSER_ASH_PSI_MEMORY_METRICS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/metrics/psi_memory_parser.h"

namespace ash {

// PSIMemoryMetrics is a background service that periodically
// retrieves memory pressure stall information from ChromeOS and publishes that
// information as UMA metrics, so that we can know the impact of upcoming
// features by examining the memory pressure histograms in dashboards
// with and without a candidate feature.
// Background: PSI (Pressure Stall Information) is a measure of the execution
// stalls that happen while waiting for memory/paging operatios (thrashing),
// and is a widely acceptable method of measuring the impact of low-memory
// stations (ref.: https://lwn.net/Articles/759781/ )
class PSIMemoryMetrics : public base::RefCountedThreadSafe<PSIMemoryMetrics> {
 public:
  explicit PSIMemoryMetrics(uint32_t period);

  PSIMemoryMetrics(const PSIMemoryMetrics&) = delete;
  PSIMemoryMetrics& operator=(const PSIMemoryMetrics&) = delete;
  PSIMemoryMetrics() = delete;

  // Begins data collection.
  void Start();

  // Ends data collection.
  void Stop();

 private:
  ~PSIMemoryMetrics();

  // Friend it so it can see private members for testing
  friend class PSIMemoryMetricsTest;
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, SunnyDay1);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, SunnyDay2);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, SunnyDay3);

  static scoped_refptr<PSIMemoryMetrics> CreateForTesting(
      uint32_t period,
      const std::string& testfilename) {
    scoped_refptr<PSIMemoryMetrics> rv =
        base::MakeRefCounted<PSIMemoryMetrics>(period);
    rv->memory_psi_file_ = testfilename;
    return rv;
  }

  // Friend it so it can call our private destructor.
  friend class base::RefCountedThreadSafe<PSIMemoryMetrics>;

  void CollectEvents();

  // Schedules a repeating timer to drive metric collection in the future.
  void ScheduleCollector();

  // Cancels the running timer from the same sequence the timer runs in.
  void CancelTimer();

  // Interval between metrics collection.
  std::string memory_psi_file_;
  metrics::PSIMemoryParser parser_;
  base::TimeDelta collection_interval_;

  // The background task runner where the collection takes place.
  scoped_refptr<base::SequencedTaskRunner> runner_;

  // The timer that schedules the collection on a regular interval.
  std::unique_ptr<base::RepeatingTimer> timer_
      GUARDED_BY_CONTEXT(background_sequence_checker_);

  SEQUENCE_CHECKER(background_sequence_checker_);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PSI_MEMORY_METRICS_H_
