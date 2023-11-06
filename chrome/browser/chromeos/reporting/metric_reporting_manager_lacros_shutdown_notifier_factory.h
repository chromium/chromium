// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_SHUTDOWN_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_SHUTDOWN_NOTIFIER_FACTORY_H_

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_context.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

namespace reporting::metrics {

// Factory implementation that notifies subscribers of the
// `MetricReportingManagerLacros` component shutdown for a given context.
// Primarily used by metric reporting components in Lacros to perform cleanup
// tasks on shutdown.
class MetricReportingManagerLacrosShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  // Static helper that returns the singleton instance of the
  // `MetricReportingManagerLacrosShutdownNotifierFactory`.
  static MetricReportingManagerLacrosShutdownNotifierFactory* GetInstance();

  MetricReportingManagerLacrosShutdownNotifierFactory(
      const MetricReportingManagerLacrosShutdownNotifierFactory& other) =
      delete;
  MetricReportingManagerLacrosShutdownNotifierFactory& operator=(
      const MetricReportingManagerLacrosShutdownNotifierFactory& other) =
      delete;

 private:
  friend base::NoDestructor<
      MetricReportingManagerLacrosShutdownNotifierFactory>;

  MetricReportingManagerLacrosShutdownNotifierFactory();
  ~MetricReportingManagerLacrosShutdownNotifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  // Selects the proper context to use based on the mapping in
  // `profile_selections_`.
  ::content::BrowserContext* GetBrowserContextToUse(
      ::content::BrowserContext* context) const override;

  const ProfileSelections profile_selections_;
};

}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_SHUTDOWN_NOTIFIER_FACTORY_H_
