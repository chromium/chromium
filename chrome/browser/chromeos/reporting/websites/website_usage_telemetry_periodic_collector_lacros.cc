// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_usage_telemetry_periodic_collector_lacros.h"

#include "base/functional/bind.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_shutdown_notifier_factory.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

WebsiteUsageTelemetryPeriodicCollectorLacros::
    WebsiteUsageTelemetryPeriodicCollectorLacros(
        Profile* profile,
        Sampler* sampler,
        MetricReportQueue* metric_report_queue,
        ReportingSettings* reporting_settings)
    : UsageTelemetryPeriodicCollectorBase(
          sampler,
          metric_report_queue,
          reporting_settings,
          /*rate_setting_path=*/kReportWebsiteTelemetryCollectionRateMs,
          /*default_rate=*/metrics::kDefaultWebsiteTelemetryCollectionRate) {
  CHECK(profile);

  // The metric reporting manager will own the periodic collector, so the
  // following subscription is guaranteed to be valid with the registered
  // callback being triggered right before the metric reporting manager shuts
  // down and destroys the collector.
  metric_reporting_manager_shutdown_subscription_ =
      metrics::MetricReportingManagerLacrosShutdownNotifierFactory::
          GetInstance()
              ->Get(profile)
              ->Subscribe(base::BindRepeating(
                  &WebsiteUsageTelemetryPeriodicCollectorLacros::
                      OnMetricReportingManagerShutdown,
                  base::Unretained(this)));
}

WebsiteUsageTelemetryPeriodicCollectorLacros::
    ~WebsiteUsageTelemetryPeriodicCollectorLacros() = default;

void WebsiteUsageTelemetryPeriodicCollectorLacros::
    OnMetricReportingManagerShutdown() {
  // Make an attempt to collect any usage data that was recently recorded from
  // the `WebsiteUsageObserver` so we can prevent data staleness should the
  // profile be inaccessible for too long.
  Collect(/*is_event_driven=*/false);
}

}  // namespace reporting
