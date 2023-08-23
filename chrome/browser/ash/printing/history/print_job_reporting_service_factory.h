// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class PrintJobReportingService;

// Singleton that owns all PrintJobReportingServices and associates them with
// Profiles.
class PrintJobReportingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the PrintJobReportingService for |context|, creating it if it is
  // not yet created.
  static PrintJobReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static PrintJobReportingServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<PrintJobReportingServiceFactory>;

  PrintJobReportingServiceFactory();
  PrintJobReportingServiceFactory(const PrintJobReportingServiceFactory&) =
      delete;
  PrintJobReportingServiceFactory& operator=(
      const PrintJobReportingServiceFactory&) = delete;
  ~PrintJobReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_REPORTING_SERVICE_FACTORY_H_
