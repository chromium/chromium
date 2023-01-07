// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_reporting {

// static
CloudProfileReportingServiceFactory*
CloudProfileReportingServiceFactory::GetInstance() {
  return base::Singleton<CloudProfileReportingServiceFactory>::get();
}

// static
CloudProfileReportingService*
CloudProfileReportingServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CloudProfileReportingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

KeyedService* CloudProfileReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return new CloudProfileReportingService(
      profile,
      g_browser_process->browser_policy_connector()
          ->device_management_service(),
      g_browser_process->shared_url_loader_factory());
}
bool CloudProfileReportingServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

CloudProfileReportingServiceFactory::CloudProfileReportingServiceFactory()
    : ProfileKeyedServiceFactory("CloudProfileReporting",
                                 ProfileSelections::BuildForRegularProfile()) {}

CloudProfileReportingServiceFactory::~CloudProfileReportingServiceFactory() =
    default;

}  // namespace enterprise_reporting
