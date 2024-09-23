// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTERNAL_METRICS_EXTERNAL_METRICS_H_
#define CHROME_BROWSER_ASH_EXTERNAL_METRICS_EXTERNAL_METRICS_H_

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
  // The directory containing per-pid event files from which externally-reported
  // metrics are read.
  static constexpr char kEventsDirectoryPath[] =
      "/var/lib/metrics/uma-events.d";
  // This directory holds per-pid event files. These files contain metrics
  // collected before the stateful partition is mounted. This is read
  // by externally-reported metrics when the crash reporter is invoked with the
  // FLAGS_early flag to capture early boot metrics.
  static constexpr char kUmaEarlyMetricsDirectoryPath[] = "/run/early-metrics/";

  ExternalMetrics();

  ExternalMetrics(const ExternalMetrics&) = delete;
  ExternalMetrics& operator=(const ExternalMetrics&) = delete;

  // Returns the global singleton or null if it hasn't been created.
  static ExternalMetrics* Get();

  // Begins the external data collection.  This service is started and stopped
  // by the chrome metrics service.  Calls to RecordAction originate in the
  // blocking pool but are executed in the UI thread.
  void Start();

  // Creates an ExternalMetrics instance reading from |filename| and |dir| for
  // testing purpose.
  static scoped_refptr<ExternalMetrics> CreateForTesting(
      const std::string& filename,
      const std::string& uma_events_dir,
      const std::string& uma_early_metrics_dir);

  // Synchronously executes one collection run for testing purposes. Does not
  // schedule any followup runs.
  void CollectNowForTesting() { CollectEvents(); }

 private:
  friend class base::RefCountedThreadSafe<ExternalMetrics>;
  friend class ExternalMetricsTest;

  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, CustomInterval);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, ProcessSamplesSuccessfully);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, HandleMissingFile);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest, CanReceiveHistogram);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest,
                           CanReceiveHistogramFromPidFiles);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest,
                           CanReceiveMetricsFromEarlyMetricsDir);
  FRIEND_TEST_ALL_PREFIXES(ExternalMetricsTest,
                           IncorrectHistogramsAreDiscarded);

  // The max length of a message (name-value pair, plus header)
  static const int kMetricsMessageMaxLength = 1024;  // be generous

  ~ExternalMetrics();

  // Passes an action event to the UMA service on the UI thread.
  void RecordActionUI(const std::string& action_string, int num_samples);

  // Passes an action event to the UMA service.
  void RecordAction(const metrics::MetricSample& sample);

  // Records an external crash of the given string description to
  // UMA service on the UI thread.
  void RecordCrashUI(const std::string& crash_kind, int num_samples);

  // Records an external crash of the given string description.
  void RecordCrash(const metrics::MetricSample& sample);

  // Records an histogram. |sample| is expected to be an histogram.
  void RecordHistogram(const metrics::MetricSample& sample);

  // Records a sparse histogram. |sample| is expected to be a sparse histogram.
  void RecordSparseHistogram(const metrics::MetricSample& sample);

  // Records a linear histogram. |sample| is expected to be a linear histogram.
  void RecordLinearHistogram(const metrics::MetricSample& sample);

  // Returns the number of samples found in |samples|. |samples| is
  // cleared at the end of the method. Used to process the samples in an event
  // file.
  int ProcessSamples(
      std::vector<std::unique_ptr<metrics::MetricSample>>* samples);

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

  // Directory of per-pid event files used by libmetrics to send metrics to
  // Chrome.
  std::string uma_events_dir_;

  // Directory of per-pid event files before stateful partition is mounted.
  // Used by libmetrics to send metrics to Chrome.
  std::string uma_early_metrics_dir_;

  // Interval between metrics being read from our sources.
  base::TimeDelta collection_interval_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTERNAL_METRICS_EXTERNAL_METRICS_H_
