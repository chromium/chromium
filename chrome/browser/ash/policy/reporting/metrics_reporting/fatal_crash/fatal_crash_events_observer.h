// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_

#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

// Observes fatal crash events.
class FatalCrashEventsObserver
    : public MojoServiceEventsObserverBase<
          ash::cros_healthd::mojom::EventObserver>,
      public ash::cros_healthd::mojom::EventObserver {
 public:
  FatalCrashEventsObserver();

  FatalCrashEventsObserver(const FatalCrashEventsObserver& other) = delete;
  FatalCrashEventsObserver& operator=(const FatalCrashEventsObserver& other) =
      delete;

  ~FatalCrashEventsObserver() override;

 private:
  MetricData FillFatalCrashTelemetry(
      const ::ash::cros_healthd::mojom::CrashEventInfoPtr& info);

  // ash::cros_healthd::mojom::EventObserver:
  void OnEvent(const ash::cros_healthd::mojom::EventInfoPtr info) override;

  // CrosHealthdEventsObserverBase
  void AddObserver() override;
};
}  // namespace reporting
#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
