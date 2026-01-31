// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_controller_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/pref_url_list_matcher.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"

namespace enterprise_reporting {

// static
SaasUsageReportingController*
SaasUsageReportingControllerFactory::GetForProfile(Profile* profile) {
  return static_cast<SaasUsageReportingController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SaasUsageReportingControllerFactory*
SaasUsageReportingControllerFactory::GetInstance() {
  static base::NoDestructor<SaasUsageReportingControllerFactory> instance;
  return instance.get();
}

SaasUsageReportingControllerFactory::SaasUsageReportingControllerFactory()
    : ProfileKeyedServiceFactory("SaasUsageReportingController",
                                 ProfileSelections::BuildForRegularProfile()) {}

SaasUsageReportingControllerFactory::~SaasUsageReportingControllerFactory() =
    default;

std::unique_ptr<KeyedService>
SaasUsageReportingControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SaasUsageReportingController>(
      g_browser_process->local_state(), profile->GetPrefs(),
      std::make_unique<PrefURLListMatcher>(g_browser_process->local_state(),
                                           kSaasUsageDomainUrlsForBrowser),
      std::make_unique<PrefURLListMatcher>(profile->GetPrefs(),
                                           kSaasUsageDomainUrlsForProfile));
}

}  // namespace enterprise_reporting
