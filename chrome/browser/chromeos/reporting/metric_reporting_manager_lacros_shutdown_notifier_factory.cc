// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_shutdown_notifier_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"

namespace reporting::metrics {

// static
MetricReportingManagerLacrosShutdownNotifierFactory*
MetricReportingManagerLacrosShutdownNotifierFactory::GetInstance() {
  static base::NoDestructor<MetricReportingManagerLacrosShutdownNotifierFactory>
      g_factory;
  return g_factory.get();
}

MetricReportingManagerLacrosShutdownNotifierFactory::
    MetricReportingManagerLacrosShutdownNotifierFactory()
    : BrowserContextKeyedServiceShutdownNotifierFactory(
          "MetricReportingManagerLacrosShutdownNotifier") {
  DependsOn(MetricReportingManagerLacrosFactory::GetInstance());
}

MetricReportingManagerLacrosShutdownNotifierFactory::
    ~MetricReportingManagerLacrosShutdownNotifierFactory() = default;

}  // namespace reporting::metrics
