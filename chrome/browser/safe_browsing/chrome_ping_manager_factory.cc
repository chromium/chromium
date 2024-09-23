// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/chrome_v4_protocol_config_provider.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {
bool kAllowPingManagerInTests = false;
}  // namespace

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
              // Telemetry report should not be sent in guest mode.
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

ChromePingManagerFactory::~ChromePingManagerFactory() = default;

std::unique_ptr<KeyedService>
ChromePingManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<ChromeSafeBrowsingHatsDelegate> hats_delegate = nullptr;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  hats_delegate = std::make_unique<ChromeSafeBrowsingHatsDelegate>(profile);
#endif
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
      base::BindRepeating(&safe_browsing::GetPageLoadTokenForURL, profile),
      std::move(hats_delegate), /*persister_root_path=*/profile->GetPath(),
      base::BindRepeating(&ChromePingManagerFactory::ShouldSendPersistedReport,
                          profile));
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

// static
bool ChromePingManagerFactory::ShouldSendPersistedReport(Profile* profile) {
  return !profile->IsOffTheRecord() &&
         IsExtendedReportingEnabled(*profile->GetPrefs());
}

bool ChromePingManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  // PingManager is created at startup to send persisted reports.
  return true;
}

bool ChromePingManagerFactory::ServiceIsNULLWhileTesting() const {
  return !kAllowPingManagerInTests;
}

ChromePingManagerAllowerForTesting::ChromePingManagerAllowerForTesting() {
  kAllowPingManagerInTests = true;
}

ChromePingManagerAllowerForTesting::~ChromePingManagerAllowerForTesting() {
  kAllowPingManagerInTests = false;
}

}  // namespace safe_browsing
