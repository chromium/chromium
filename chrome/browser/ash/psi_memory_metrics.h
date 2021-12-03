// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PSI_MEMORY_METRICS_H_
#define CHROME_BROWSER_ASH_PSI_MEMORY_METRICS_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"

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
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, CustomInterval);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, InvalidInterval);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, SunnyDay1);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, SunnyDay2);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, SunnyDay3);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, InternalsA);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, InternalsB);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, InternalsC);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, InternalsD);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryMetricsTest, InternalsE);

  // Enumeration representing success and various failure modes for parsing PSI
  // memory data. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class ParsePSIMemStatus {
    kSuccess,
    kReadFileFailed,
    kUnexpectedDataFormat,
    kInvalidMetricFormat,
    kParsePSIValueFailed,
    // Magic constant used by the histogram macros.
    kMaxValue = kParsePSIValueFailed,
  };

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

  // Retrieves one metric value from |content|, for the currently configured
  // metrics category (10, 60 or 300 seconds).
  // Only considers the substring between |start| (inclusive) and |end|
  // (exclusive).
  // Returns the floating-point string representation converted into an integer
  // which has the value multiplied by 100 - (10.20 = 1020), for
  // histogram usage.
  int GetMetricValue(const std::string& content, size_t start, size_t end);

  // Parses PSI memory pressure from  |content|, for the currently configured
  // metrics category (10, 60 or 300 seconds).
  // The some and full values are output to |metricSome| and |metricFull|,
  // respectively.
  // Returns status of the parse operation - ParsePSIMemStatus::kSuccess
  // or error code otherwise.
  ParsePSIMemStatus ParseMetrics(const std::string& content,
                                 int* metric_some,
                                 int* metric_full);

  ParsePSIMemStatus CollectEvents();

  // Calls CollectEvents and reschedules a future collection.
  void CollectEventsAndReschedule();

  // Schedules a metrics event collection in the future.
  void ScheduleCollector();

  // Cancels the running timer from the same sequence the timer runs in.
  void CancelTimer();

  // Interval between metrics collection.
  std::string memory_psi_file_;
  base::TimeDelta collection_interval_;
  std::string metric_prefix_;

  // Task controllers/monitors.
  scoped_refptr<base::SequencedTaskRunner> runner_;
  base::AtomicFlag stopped_;
  base::DelayedTaskHandle last_timer_;
};

// Items in internal are - as the name implies - NOT for outside consumption.
// Defined here to allow access to unit test.
namespace internal {

// Finds the bounds for a substring of |content| which is sandwiched between
// the given |prefix| and |suffix| indices. Search only considers
// the portion of the string starting from |search_start|.
// Returns false if the prefix and/or suffix are not found, true otherwise.
// |start| and |end| are output parameters populated with the indices
// for the middle string.
bool FindMiddleString(const base::StringPiece& content,
                      size_t search_start,
                      const base::StringPiece& prefix,
                      const base::StringPiece& suffix,
                      size_t* start,
                      size_t* end);

}  // namespace internal

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PSI_MEMORY_METRICS_H_
