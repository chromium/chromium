// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

namespace safe_browsing {

// static
RealTimeUrlLookupService* RealTimeUrlLookupServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RealTimeUrlLookupService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
RealTimeUrlLookupServiceFactory*
RealTimeUrlLookupServiceFactory::GetInstance() {
  return base::Singleton<RealTimeUrlLookupServiceFactory>::get();
}

RealTimeUrlLookupServiceFactory::RealTimeUrlLookupServiceFactory()
    : ProfileKeyedServiceFactory(
          "RealTimeUrlLookupService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(SafeBrowsingNavigationObserverManagerFactory::GetInstance());
#if BUILDFLAG(FULL_SAFE_BROWSING)
  DependsOn(AdvancedProtectionStatusManagerFactory::GetInstance());
#endif
  DependsOn(NetworkContextServiceFactory::GetInstance());
}

KeyedService* RealTimeUrlLookupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
              profile));
  return new RealTimeUrlLookupService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      VerdictCacheManagerFactory::GetForProfile(profile),
      base::BindRepeating(
          &safe_browsing::GetUserPopulationForProfileWithCookieTheftExperiments,
          profile),
      profile->GetPrefs(),
      std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile)),
      base::BindRepeating(&safe_browsing::SyncUtils::
                              AreSigninAndSyncSetUpForSafeBrowsingTokenFetches,
                          SyncServiceFactory::GetForProfile(profile),
                          IdentityManagerFactory::GetForProfile(profile)),
      profile->IsOffTheRecord(), g_browser_process->variations_service(),
      SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
          profile));
}

}  // namespace safe_browsing
