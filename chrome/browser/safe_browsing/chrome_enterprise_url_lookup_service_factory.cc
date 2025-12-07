// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/connectors/core/content_area_user_provider.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/core/browser/realtime/chrome_enterprise_url_lookup_service.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "url/gurl.h"

namespace {

// Helper function for retrieving the email associated with `profile`.
// Makes it easier to bind a callback to
// `enterprise_connectors::GetProfileEmail` which has multiple overloads, so
// binding to it would require a cast.
std::string GetProfileEmail(Profile* profile) {
  return enterprise_connectors::GetProfileEmail(profile);
}

// Returns true if the policy command line switch can be used.
bool IsCommandLineSwitchSupported() {
  return g_browser_process && g_browser_process->browser_policy_connector()
                                  ->IsCommandLineSwitchSupported();
}

}  // namespace

namespace safe_browsing {

// static
RealTimeUrlLookupServiceBase*
ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RealTimeUrlLookupServiceBase*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
ChromeEnterpriseRealTimeUrlLookupServiceFactory*
ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<ChromeEnterpriseRealTimeUrlLookupServiceFactory>
      instance;
  return instance.get();
}

ChromeEnterpriseRealTimeUrlLookupServiceFactory::
    ChromeEnterpriseRealTimeUrlLookupServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChromeEnterpriseRealTimeUrlLookupService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Enterprise real time URL check can be enabled in guest profile.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
  DependsOn(SafeBrowsingNavigationObserverManagerFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(policy::ManagementServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> ChromeEnterpriseRealTimeUrlLookupServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          profile->GetURLLoaderFactory());
  return std::make_unique<ChromeEnterpriseRealTimeUrlLookupService>(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      VerdictCacheManagerFactory::GetForProfile(profile),
      base::BindRepeating(&safe_browsing::GetUserPopulationForProfile, profile),
      std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile)),
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile),
      SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
          profile),
      profile->GetPrefs(),
      /*webui_delegate=*/WebUIContentInfoSingleton::GetInstance(),
      IdentityManagerFactory::GetForProfile(profile),
      policy::ManagementServiceFactory::GetForProfile(profile),
      profile->IsOffTheRecord(), profile->IsGuestSession(),
      base::BindRepeating(&GetProfileEmail, profile),
      base::BindRepeating(&enterprise_connectors::GetActiveContentAreaUser,
                          IdentityManagerFactory::GetForProfile(profile)),
      base::BindRepeating(&enterprise_util::IsProfileAffiliated, profile),
      /*is_command_line_switch_supported=*/IsCommandLineSwitchSupported());
}

}  // namespace safe_browsing
