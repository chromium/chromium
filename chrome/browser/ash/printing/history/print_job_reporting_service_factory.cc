// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_reporting_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/history/print_job_reporting_service.h"

namespace ash {

// static
PrintJobReportingService* PrintJobReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrintJobReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrintJobReportingServiceFactory*
PrintJobReportingServiceFactory::GetInstance() {
  static base::NoDestructor<PrintJobReportingServiceFactory> instance;
  return instance.get();
}

PrintJobReportingServiceFactory::PrintJobReportingServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrintJobReportingServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

PrintJobReportingServiceFactory::~PrintJobReportingServiceFactory() = default;

std::unique_ptr<KeyedService>
PrintJobReportingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return PrintJobReportingService::Create();
}

bool PrintJobReportingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
