// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MEMORY_METRICS_H_
#define CHROME_BROWSER_ASH_MEMORY_METRICS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/memory/memory.h"
#include "components/metrics/psi_memory_parser.h"

namespace ash {

// MemoryMetrics is a background service that periodically publishes memory
// metrics. This class is responsible for retrieving memory pressure stall
// information. The class ZramMetrics is responsible for retrieving zram-related
// information.
// Background: PSI (Pressure Stall Information) is a measure of the execution
// stalls that happen while waiting for memory/paging operatios (thrashing),
// and is a widely acceptable method of measuring the impact of low-memory
// stations (ref.: https://lwn.net/Articles/759781/ )
class MemoryMetrics : public base::RefCountedThreadSafe<MemoryMetrics> {
 public:
  static constexpr int kDefaultPeriodInSeconds = 10;
  explicit MemoryMetrics(uint32_t period);

  MemoryMetrics(const MemoryMetrics&) = delete;
  MemoryMetrics& operator=(const MemoryMetrics&) = delete;
  MemoryMetrics() = delete;

  // Begins data collection.
  void Start();

  // Ends data collection.
  void Stop();

 private:
  ~MemoryMetrics();

  // Friend it so it can see private members for testing.
  friend class MemoryMetricsTest;
  FRIEND_TEST_ALL_PREFIXES(MemoryMetricsTest, SunnyDay1);
  FRIEND_TEST_ALL_PREFIXES(MemoryMetricsTest, SunnyDay2);
  FRIEND_TEST_ALL_PREFIXES(MemoryMetricsTest, SunnyDay3);

  static scoped_refptr<MemoryMetrics> CreateForTesting(
      uint32_t period,
      const std::string& testfilename) {
    scoped_refptr<MemoryMetrics> rv =
        base::MakeRefCounted<MemoryMetrics>(period);
    rv->memory_psi_file_ = testfilename;
    return rv;
  }

  // Friend it so it can call our private destructor.
  friend class base::RefCountedThreadSafe<MemoryMetrics>;

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

  // The class that is responsible for emitting zram metrics.
  scoped_refptr<memory::ZramMetrics> zram_metrics_;

  SEQUENCE_CHECKER(background_sequence_checker_);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MEMORY_METRICS_H_
