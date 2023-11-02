// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
ChromePasswordProtectionService*
ChromePasswordProtectionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromePasswordProtectionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
ChromePasswordProtectionServiceFactory*
ChromePasswordProtectionServiceFactory::GetInstance() {
  return base::Singleton<ChromePasswordProtectionServiceFactory>::get();
}

ChromePasswordProtectionServiceFactory::ChromePasswordProtectionServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChromePasswordProtectionService",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(PasswordReuseManagerFactory::GetInstance());
  DependsOn(browser_sync::UserEventServiceFactory::GetInstance());
  DependsOn(SafeBrowsingNavigationObserverManagerFactory::GetInstance());
}

KeyedService* ChromePasswordProtectionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!g_browser_process->safe_browsing_service()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return new ChromePasswordProtectionService(
      g_browser_process->safe_browsing_service(), profile);
}

}  // namespace safe_browsing
