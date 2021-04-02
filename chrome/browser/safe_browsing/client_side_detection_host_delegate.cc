// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/db/database_manager.h"

namespace safe_browsing {

namespace {
// The number of user gestures we trace back for CSD attribution.
const int kCSDAttributionUserGestureLimitForExtendedReporting = 5;
}  // namespace

// static
std::unique_ptr<ClientSideDetectionHost>
ClientSideDetectionHostDelegate::CreateHost(content::WebContents* tab) {
  content::BrowserContext* browser_context = tab->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return ClientSideDetectionHost::Create(
      tab, std::make_unique<ClientSideDetectionHostDelegate>(tab),
      profile->GetPrefs(),
      std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile)),
      profile->IsOffTheRecord(),
      base::BindRepeating(&safe_browsing::SyncUtils::IsPrimaryAccountSignedIn,
                          IdentityManagerFactory::GetForProfile(profile)));
}

ClientSideDetectionHostDelegate::ClientSideDetectionHostDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  navigation_observer_manager_ =
      GetSafeBrowsingNavigationObserverManager().get();
}
ClientSideDetectionHostDelegate::~ClientSideDetectionHostDelegate() = default;

bool ClientSideDetectionHostDelegate::HasSafeBrowsingUserInteractionObserver() {
  return SafeBrowsingUserInteractionObserver::FromWebContents(web_contents_);
}

PrefService* ClientSideDetectionHostDelegate::GetPrefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile ? profile->GetPrefs() : nullptr;
}

scoped_refptr<SafeBrowsingDatabaseManager>
ClientSideDetectionHostDelegate::GetSafeBrowsingDBManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->database_manager().get() : nullptr;
}

scoped_refptr<SafeBrowsingNavigationObserverManager>
ClientSideDetectionHostDelegate::GetSafeBrowsingNavigationObserverManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->navigation_observer_manager().get() : nullptr;
}

scoped_refptr<BaseUIManager>
ClientSideDetectionHostDelegate::GetSafeBrowsingUIManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->ui_manager() : nullptr;
}

ClientSideDetectionService*
ClientSideDetectionHostDelegate::GetClientSideDetectionService() {
  return ClientSideDetectionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void ClientSideDetectionHostDelegate::AddReferrerChain(
    ClientPhishingRequest* verdict,
    GURL current_url) {
  if (!navigation_observer_manager_) {
    return;
  }
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager_->IdentifyReferrerChainByEventURL(
          current_url, SessionID::InvalidValue(),
          kCSDAttributionUserGestureLimitForExtendedReporting,
          verdict->mutable_referrer_chain());

  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.CSDAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  // Determines how many recent navigation events to append to referrer
  // chain if any.
  size_t recent_navigations_to_collect =
      CountOfRecentNavigationsToAppend(result);

  navigation_observer_manager_->AppendRecentNavigations(
      recent_navigations_to_collect, verdict->mutable_referrer_chain());
}

size_t ClientSideDetectionHostDelegate::CountOfRecentNavigationsToAppend(
    SafeBrowsingNavigationObserverManager::AttributionResult result) {
  return web_contents_ ? SafeBrowsingNavigationObserverManager::
                             CountOfRecentNavigationsToAppend(
                                 *Profile::FromBrowserContext(
                                     web_contents_->GetBrowserContext()),
                                 result)
                       : 0u;
}

}  // namespace safe_browsing
