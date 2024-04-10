// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/reporting_service.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace data_controls {

// -------------------------------
// ReportingService implementation
// -------------------------------

ReportingService::ReportingService(content::BrowserContext& browser_context)
    : profile_(*Profile::FromBrowserContext(&browser_context)) {}

ReportingService::~ReportingService() = default;

// --------------------------------------
// ReportingServiceFactory implementation
// --------------------------------------

// static
ReportingService* ReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
ReportingServiceFactory* ReportingServiceFactory::GetInstance() {
  static base::NoDestructor<ReportingServiceFactory> instance;
  return instance.get();
}

ReportingServiceFactory::ReportingServiceFactory()
    : ProfileKeyedServiceFactory(
          "DataControlsReportingService",
          // `kOriginalOnly` is used since there is no reporting done for
          // incognito profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

ReportingServiceFactory::~ReportingServiceFactory() = default;

std::unique_ptr<KeyedService>
ReportingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return base::WrapUnique(new ReportingService(*context));
}

}  // namespace data_controls
