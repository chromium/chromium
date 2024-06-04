// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_reporting {

// static
CloudProfileReportingServiceFactory*
CloudProfileReportingServiceFactory::GetInstance() {
  static base::NoDestructor<CloudProfileReportingServiceFactory> instance;
  return instance.get();
}

// static
CloudProfileReportingService*
CloudProfileReportingServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CloudProfileReportingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

std::unique_ptr<KeyedService>
CloudProfileReportingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<CloudProfileReportingService>(profile);
}
bool CloudProfileReportingServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

CloudProfileReportingServiceFactory::CloudProfileReportingServiceFactory()
    : ProfileKeyedServiceFactory("CloudProfileReporting",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(enterprise::ProfileIdServiceFactory::GetInstance());
}

CloudProfileReportingServiceFactory::~CloudProfileReportingServiceFactory() =
    default;

}  // namespace enterprise_reporting
