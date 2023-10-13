// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_PERIODIC_COLLECTOR_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_PERIODIC_COLLECTOR_LACROS_H_

#include "base/callback_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/reporting/usage_telemetry_periodic_collector_base.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

namespace reporting {

// Periodic collector that collects and reports website usage telemetry data
// originally collected by the `WebsiteUsageObserver`. This is a standalone
// component in Lacros that is similar to the `PeriodicCollector` but only
// controls the collection rate based on respective policy setting. This
// component also monitors the shutdown of the `MetricReportingManagerLacros`
// component via the `MetricReportingManagerLacrosShutdownNotifierFactory` so it
// can make a best-effort attempt to enqueue collected website usage telemetry
// before it is stale. This is because the observer does not associate usage
// data with a timestamp today.
class WebsiteUsageTelemetryPeriodicCollectorLacros
    : public UsageTelemetryPeriodicCollectorBase {
 public:
  WebsiteUsageTelemetryPeriodicCollectorLacros(
      Profile* profile,
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings);
  WebsiteUsageTelemetryPeriodicCollectorLacros(
      const WebsiteUsageTelemetryPeriodicCollectorLacros& other) = delete;
  WebsiteUsageTelemetryPeriodicCollectorLacros& operator=(
      const WebsiteUsageTelemetryPeriodicCollectorLacros& other) = delete;
  ~WebsiteUsageTelemetryPeriodicCollectorLacros() override;

 private:
  // Triggered on shutdown of the `MetricReportingManagerLacros` component so
  // we can enqueue collected data (if any) before it is stale.
  void OnMetricReportingManagerShutdown();

  // Callback subscription that is used to monitor the shutdown of the
  // `MetricReportingManagerLacros` component.
  base::CallbackListSubscription
      metric_reporting_manager_shutdown_subscription_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_PERIODIC_COLLECTOR_LACROS_H_
