// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

namespace reporting::metrics {

// Factory implementation for the `MetricReportingManagerLacros` for a given
// profile.
class MetricReportingManagerLacrosFactory : public ProfileKeyedServiceFactory {
 public:
  // Static helper that returns the singleton instance of the
  // `MetricReportingManagerLacrosFactory`.
  static MetricReportingManagerLacrosFactory* GetInstance();

  // Returns an instance of `MetricReportingManagerLacros` for the
  // given profile.
  static MetricReportingManagerLacros* GetForProfile(Profile* profile);

  MetricReportingManagerLacrosFactory(
      const MetricReportingManagerLacrosFactory&) = delete;
  MetricReportingManagerLacrosFactory& operator=(
      const MetricReportingManagerLacrosFactory&) = delete;

 private:
  friend base::NoDestructor<MetricReportingManagerLacrosFactory>;

  MetricReportingManagerLacrosFactory();
  ~MetricReportingManagerLacrosFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      ::content::BrowserContext* context) const override;
};

}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_FACTORY_H_
