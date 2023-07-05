// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_getter_factory.h"

#include "chrome/browser/ip_protection/ip_protection_auth_token_getter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"

// static
IpProtectionAuthTokenGetter* IpProtectionAuthTokenGetterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IpProtectionAuthTokenGetter*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IpProtectionAuthTokenGetterFactory*
IpProtectionAuthTokenGetterFactory::GetInstance() {
  static base::NoDestructor<IpProtectionAuthTokenGetterFactory> instance;
  return instance.get();
}

// static
ProfileSelections
IpProtectionAuthTokenGetterFactory::CreateProfileSelections() {
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

IpProtectionAuthTokenGetterFactory::IpProtectionAuthTokenGetterFactory()
    : ProfileKeyedServiceFactory("IpProtectionAuthTokenGetterFactory",
                                 CreateProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IpProtectionAuthTokenGetterFactory::~IpProtectionAuthTokenGetterFactory() =
    default;

std::unique_ptr<KeyedService>
IpProtectionAuthTokenGetterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<IpProtectionAuthTokenGetter>(
      IdentityManagerFactory::GetForProfile(profile));
}

bool IpProtectionAuthTokenGetterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // TODO(https://crbug.com/1444621): If we update IpProtectionAuthTokenGetter
  // to begin requesting tokens on construction, have this return true to
  // instantiate an instance of the IpProtectionAuthTokenGetter when the
  // BrowserContext is created instead of lazily so that it can begin fetching
  // tokens as soon as possible.
  return false;
}
