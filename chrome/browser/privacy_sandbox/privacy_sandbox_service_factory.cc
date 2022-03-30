// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/storage_partition.h"

PrivacySandboxServiceFactory* PrivacySandboxServiceFactory::GetInstance() {
  return base::Singleton<PrivacySandboxServiceFactory>::get();
}

PrivacySandboxService* PrivacySandboxServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacySandboxService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxServiceFactory::PrivacySandboxServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrivacySandboxService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PrivacySandboxSettingsFactory::GetInstance());
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(browsing_topics::BrowsingTopicsServiceFactory::GetInstance());
}

KeyedService* PrivacySandboxServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PrivacySandboxService(
      PrivacySandboxSettingsFactory::GetForProfile(profile),
      CookieSettingsFactory::GetForProfile(profile).get(), profile->GetPrefs(),
      profile->GetProfilePolicyConnector()->policy_service(),
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()->GetInterestGroupManager(),
      profile_metrics::GetBrowserProfileType(profile),
      (!profile->IsGuestSession() || profile->IsOffTheRecord())
          ? profile->GetBrowsingDataRemover()
          : nullptr,
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile));
}

content::BrowserContext* PrivacySandboxServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // TODO(crbug.com/1284295): Determine whether this actually needs to be
  // created, or whether all usage in OTR contexts can be removed.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
