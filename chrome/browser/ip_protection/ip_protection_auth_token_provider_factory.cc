// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_provider_factory.h"

#include "chrome/browser/ip_protection/ip_protection_auth_token_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
IpProtectionAuthTokenProvider*
IpProtectionAuthTokenProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<IpProtectionAuthTokenProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IpProtectionAuthTokenProviderFactory*
IpProtectionAuthTokenProviderFactory::GetInstance() {
  static base::NoDestructor<IpProtectionAuthTokenProviderFactory> instance;
  return instance.get();
}

// static
ProfileSelections
IpProtectionAuthTokenProviderFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }
  // IP Protection usage requires that a Gaia account is available when
  // authenticating to the proxy (to prevent it from being abused). For
  // incognito mode, use the profile associated with the logged in user since
  // users will have a more private experience with IP Protection enabled.
  // Skip other profile types like Guest and System where no Gaia is available.
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

IpProtectionAuthTokenProviderFactory::IpProtectionAuthTokenProviderFactory()
    : ProfileKeyedServiceFactory("IpProtectionAuthTokenProviderFactory",
                                 CreateProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IpProtectionAuthTokenProviderFactory::~IpProtectionAuthTokenProviderFactory() =
    default;

std::unique_ptr<KeyedService>
IpProtectionAuthTokenProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<IpProtectionAuthTokenProvider>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

bool IpProtectionAuthTokenProviderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // TODO(https://crbug.com/1444621): If we update IpProtectionAuthTokenProvider
  // to begin requesting tokens on construction, have this return true to
  // instantiate an instance of the IpProtectionAuthTokenProvider when the
  // BrowserContext is created instead of lazily so that it can begin fetching
  // tokens as soon as possible.
  return false;
}
