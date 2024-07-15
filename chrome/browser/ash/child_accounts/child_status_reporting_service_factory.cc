// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service.h"

namespace ash {

// static
ChildStatusReportingService*
ChildStatusReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChildStatusReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChildStatusReportingServiceFactory*
ChildStatusReportingServiceFactory::GetInstance() {
  static base::NoDestructor<ChildStatusReportingServiceFactory> factory;
  return factory.get();
}

ChildStatusReportingServiceFactory::ChildStatusReportingServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChildStatusReportingServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

ChildStatusReportingServiceFactory::~ChildStatusReportingServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ChildStatusReportingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChildStatusReportingService>(context);
}

}  // namespace ash
