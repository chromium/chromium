// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
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
  static base::NoDestructor<TailoredSecurityServiceFactory> instance;
  return instance.get();
}

TailoredSecurityServiceFactory::TailoredSecurityServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingTailoredSecurityService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TailoredSecurityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeTailoredSecurityService>(
      Profile::FromBrowserContext(context));
}

bool TailoredSecurityServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool TailoredSecurityServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace safe_browsing
