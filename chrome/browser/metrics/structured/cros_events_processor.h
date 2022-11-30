// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_

#include "components/metrics/structured/events_processor_interface.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::structured::cros_event {

class CrOSEventsProcessor : public EventsProcessorInterface {
 public:
  ~CrOSEventsProcessor() override;

  // Registers device-level prefs.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers user-level prefs.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  bool ShouldProcessOnEventRecord(const Event& event) override;
  void OnEventsRecord(Event* event) override;
};

}  // namespace metrics::structured::cros_event

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_CROS_EVENTS_PROCESSOR_H_
