// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/chrome_v4_protocol_config_provider.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

// static
ChromePingManagerFactory* ChromePingManagerFactory::GetInstance() {
  static base::NoDestructor<ChromePingManagerFactory> instance;
  return instance.get();
}

// static
PingManager* ChromePingManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PingManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

ChromePingManagerFactory::ChromePingManagerFactory()
    : ProfileKeyedServiceFactory(
          "ChromeSafeBrowsingPingManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

ChromePingManagerFactory::~ChromePingManagerFactory() = default;

KeyedService* ChromePingManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return PingManager::Create(
      GetV4ProtocolConfig(),
      g_browser_process->safe_browsing_service()->GetURLLoaderFactory(profile),
      std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile)),
      base::BindRepeating(
          &ChromePingManagerFactory::ShouldFetchAccessTokenForReport, profile),
      safe_browsing::WebUIInfoSingleton::GetInstance(),
      content::GetUIThreadTaskRunner({}),
      base::BindRepeating(&safe_browsing::GetUserPopulationForProfile, profile),
      base::FeatureList::IsEnabled(
          safe_browsing::kAddPageLoadTokenToClientSafeBrowsingReport)
          ? base::BindRepeating(&safe_browsing::GetPageLoadTokenForURL, profile)
          : base::NullCallback());
}

// static
bool ChromePingManagerFactory::ShouldFetchAccessTokenForReport(
    Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return IsEnhancedProtectionEnabled(*prefs) && identity_manager &&
         safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(identity_manager);
}

}  // namespace safe_browsing
