// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
AdvancedProtectionStatusManager*
AdvancedProtectionStatusManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<AdvancedProtectionStatusManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
AdvancedProtectionStatusManagerFactory*
AdvancedProtectionStatusManagerFactory::GetInstance() {
  return base::Singleton<AdvancedProtectionStatusManagerFactory>::get();
}

AdvancedProtectionStatusManagerFactory::AdvancedProtectionStatusManagerFactory()
    : ProfileKeyedServiceFactory(
          "AdvancedProtectionStatusManager",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

AdvancedProtectionStatusManagerFactory::
    ~AdvancedProtectionStatusManagerFactory() {}

KeyedService* AdvancedProtectionStatusManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new AdvancedProtectionStatusManager(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile));
}

bool AdvancedProtectionStatusManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace safe_browsing
