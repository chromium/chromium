// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_

#include "components/metrics/structured/events_processor_interface.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::structured::cros_event {

extern const char* kResetCounterPath;

// Post-processor that will process only sequenceable events and attach metadata
// to the events.
class CrOSEventsProcessor : public EventsProcessorInterface {
 public:
  explicit CrOSEventsProcessor(const char* reset_counter_path);
  ~CrOSEventsProcessor() override;

  // Registers device-level prefs.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // EventsProcessorInterface:
  bool ShouldProcessOnEventRecord(const Event& event) override;
  void OnEventsRecord(Event* event) override;
  void OnEventRecorded(StructuredEventProto* event) override;

  void OnProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto) override;

 private:
  // The current reset counter as determined by platform2.
  int64_t current_reset_counter_ = 0;
};

}  // namespace metrics::structured::cros_event

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_
