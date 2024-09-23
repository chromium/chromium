// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"

namespace {

std::unique_ptr<KeyedService> BuildService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<safe_browsing::AdvancedProtectionStatusManager>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace

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
  static base::NoDestructor<AdvancedProtectionStatusManagerFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
AdvancedProtectionStatusManagerFactory::GetDefaultFactoryForTesting() {
  return base::BindRepeating(&BuildService);
}

AdvancedProtectionStatusManagerFactory::AdvancedProtectionStatusManagerFactory()
    : ProfileKeyedServiceFactory(
          "AdvancedProtectionStatusManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

AdvancedProtectionStatusManagerFactory::
    ~AdvancedProtectionStatusManagerFactory() = default;

std::unique_ptr<KeyedService>
AdvancedProtectionStatusManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildService(context);
}

bool AdvancedProtectionStatusManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace safe_browsing
