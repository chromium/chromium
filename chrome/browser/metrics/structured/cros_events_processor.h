// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_

#include "components/metrics/structured/events_processor_interface.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics::structured::cros_event {

// Post-processor that will process only sequenceable events and attach metadata
// to the events.
class CrOSEventsProcessor : public EventsProcessorInterface {
 public:
  explicit CrOSEventsProcessor(PrefService* pref_service);
  ~CrOSEventsProcessor() override;

  // Registers device-level prefs.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // EventsProcessorInterface:
  bool ShouldProcessOnEventRecord(const Event& event) override;
  void OnEventsRecord(Event* event) override;

  // Used only to set the current uptime to check against for testing.
  // If this value is not set explicitly, system clock will be used.
  void SetCurrentUptimeForTesting(int64_t current_uptime);

 private:
  PrefService* pref_service_;
  int64_t current_uptime_for_testing_ = 0;
};

}  // namespace metrics::structured::cros_event

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_
