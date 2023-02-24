// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
TailoredSecurityService* TailoredSecurityServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<TailoredSecurityService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
TailoredSecurityServiceFactory* TailoredSecurityServiceFactory::GetInstance() {
  return base::Singleton<TailoredSecurityServiceFactory>::get();
}

TailoredSecurityServiceFactory::TailoredSecurityServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingTailoredSecurityService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

KeyedService* TailoredSecurityServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new ChromeTailoredSecurityService(profile);
}

bool TailoredSecurityServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace safe_browsing
