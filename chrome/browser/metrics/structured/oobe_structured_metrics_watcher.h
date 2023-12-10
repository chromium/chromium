// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_OOBE_STRUCTURED_METRICS_WATCHER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_OOBE_STRUCTURED_METRICS_WATCHER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/structured/events_processor_interface.h"

namespace metrics::structured {

class StructuredMetricsService;

// A watcher class that will manual start an upload once a specified number of
// events have been recorded. Functions only during oobe.
class OobeStructuredMetricsWatcher : public EventsProcessorInterface {
 public:
  OobeStructuredMetricsWatcher(StructuredMetricsService* service,
                               int max_events);

  // EventProcessorInterface:
  bool ShouldProcessOnEventRecord(const Event& event) override;
  void OnEventsRecord(Event* event) override;
  void OnEventRecorded(StructuredEventProto* event) override;
  void OnProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto) override;
  void OnProfileAdded(const base::FilePath& path) override;

  bool IsOobeActive() const;

  bool ShouldUpload() const;

  void AttemptUpload();

 private:
  // The number of times this watcher has manually uploaded.
  int upload_count_ = 0;
  int event_count_ = 0;
  int max_events_ = 0;
  // Pointer may dangle but will never be used once it is dangling.
  raw_ptr<StructuredMetricsService, DisableDanglingPtrDetection> service_ =
      nullptr;
};
}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_OOBE_STRUCTURED_METRICS_WATCHER_H_
