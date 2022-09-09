// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTERNAL_METRICS_H_
#define CHROME_BROWSER_ASH_EXTERNAL_METRICS_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace metrics {
class MetricSample;
}  // namespace metrics

namespace ash {

// ExternalMetrics is a service that Chrome offers to Chrome OS to upload
// metrics to the UMA server on its behalf.  Chrome periodically reads the
// content of a well-know file, and parses it into name-value pairs, each
// representing a Chrome OS metrics event. The events are logged using the
// normal UMA mechanism. The file is then truncated to zero size. Chrome uses
// flock() to synchronize accesses to the file.
class ExternalMetrics : public base::RefCountedThreadSafe<ExternalMetrics> {
 public:
  // The file from which externally-reported metrics are read.
  static constexpr char kEventsFilePath[] = "/var/lib/metrics/uma-events";

  ExternalMetrics();

  ExternalMetrics(const ExternalMetrics&) = delete;
  ExternalMetrics& operator=(const ExternalMetrics&) = delete;

  // Begins the external data collection.  This service is started and stopped
  // by the chrome metrics service.  Calls to RecordAction originate in the
  // blocking pool but are executed in the UI thread.
  void Start();

  // Creates an ExternalMetrics instance reading from |filename| for testing
  // purpose.
  static scoped_refptr<ExternalMetrics> CreateForTesting(
      const std::string& filename);

 private:
  friend class base::RefCountedThreadSafe<ExternalMetrics>;
  friend class ExternalMetricsTest;

  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, CustomInterval);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, HandleMissingFile);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, CanReceiveHistogram);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest,
                           IncorrectHistogramsAreDiscarded);

  // The max length of a message (name-value pair, plus header)
  static const int kMetricsMessageMaxLength = 1024;  // be generous

  ~ExternalMetrics();

  // Passes an action event to the UMA service on the UI thread.
  void RecordActionUI(const std::string& action_string);

  // Passes an action event to the UMA service.
  void RecordAction(const std::string& action_name);

  // Records an external crash of the given string description to
  // UMA service on the UI thread.
  void RecordCrashUI(const std::string& crash_kind);

  // Records an external crash of the given string description.
  void RecordCrash(const std::string& crash_kind);

  // Records an histogram. |sample| is expected to be an histogram.
  void RecordHistogram(const metrics::MetricSample& sample);

  // Records a sparse histogram. |sample| is expected to be a sparse histogram.
  void RecordSparseHistogram(const metrics::MetricSample& sample);

  // Records a linear histogram. |sample| is expected to be a linear histogram.
  void RecordLinearHistogram(const metrics::MetricSample& sample);

  // Collects external events from metrics log file.  This is run at periodic
  // intervals.
  //
  // Returns the number of events collected.
  int CollectEvents();

  // Calls CollectEvents and reschedules a future collection.
  void CollectEventsAndReschedule();

  // Schedules a metrics event collection in the future.
  void ScheduleCollector();

  // File used by libmetrics to send metrics to Chrome.
  std::string uma_events_file_;

  // Interval between metrics being read from |uma_events_file_|.
  base::TimeDelta collection_interval_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTERNAL_METRICS_H_
