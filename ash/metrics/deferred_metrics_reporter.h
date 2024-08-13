// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_DEFERRED_METRICS_REPORTER_H_
#define ASH_METRICS_DEFERRED_METRICS_REPORTER_H_

#include <memory>
#include <string>
#include <vector>

namespace ash {

// DeferredMetricsReporter stores metrics and delays reporting them until
// `MarkReadyToReport` is called. After it becomes ready, `ReportOrSchedule`
// reports the metric immediately.
class DeferredMetricsReporter {
 public:
  // Represents a single metric.
  class Metric {
   public:
    virtual ~Metric() = default;

    // Implementers should report the metric with the given `prefix`.
    virtual void Report(const std::string& prefix) = 0;
  };

  DeferredMetricsReporter();
  DeferredMetricsReporter(const DeferredMetricsReporter& other) = delete;
  DeferredMetricsReporter& operator=(const DeferredMetricsReporter& other) =
      delete;
  ~DeferredMetricsReporter();

  // Reports the metrics immediately if this reporter is already ready.
  // Otherwise, buffers the metrics and it will be reported once it gets ready.
  void ReportOrSchedule(std::unique_ptr<Metric> metric);

  // Updates the prefix, which is used to report metrics.
  void SetPrefix(std::string new_prefix);

  // Marks this reporter as ready, and reports all metrics previously scheduled
  // if any. Prior to this, `SetPrefix` must be called with non-empty string.
  void MarkReadyToReport();

 private:
  std::string prefix_;
  std::vector<std::unique_ptr<Metric>> scheduled_metrics_;
  bool ready_ = false;
};

}  // namespace ash

#endif  // ASH_METRICS_DEFERRED_METRICS_REPORTER_H_
