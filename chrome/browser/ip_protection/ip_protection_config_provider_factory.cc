// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_config_provider_factory.h"

#include "base/command_line.h"
#include "chrome/browser/ip_protection/ip_protection_config_provider.h"
#include "chrome/browser/ip_protection/ip_protection_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
IpProtectionConfigProvider* IpProtectionConfigProviderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IpProtectionConfigProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IpProtectionConfigProviderFactory*
IpProtectionConfigProviderFactory::GetInstance() {
  static base::NoDestructor<IpProtectionConfigProviderFactory> instance;
  return instance.get();
}

// static
ProfileSelections IpProtectionConfigProviderFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableIpProtectionProxy)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }
  // IP Protection usage requires that a Gaia account is available when
  // authenticating to the proxy (to prevent it from being abused). For
  // incognito mode where there's not an account available by default, use the
  // profile associated with the logged in user if there is one. There's a small
  // privacy trade-off with this, the downside being that the incognito mode
  // profile will send an OAuth token associated with the user to the proxy
  // token provider server periodically as new blinded proxy tokens are needed,
  // and users might not expect this behavior. The privacy benefits of being
  // able to use IP Protection in incognito mode should far outweigh this,
  // though. Skip other profile types like Guest and System where no Gaia is
  // available.
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

IpProtectionConfigProviderFactory::IpProtectionConfigProviderFactory()
    : ProfileKeyedServiceFactory("IpProtectionConfigProviderFactory",
                                 CreateProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IpProtectionConfigProviderFactory::~IpProtectionConfigProviderFactory() =
    default;

std::unique_ptr<KeyedService>
IpProtectionConfigProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<IpProtectionConfigProvider>(
      IdentityManagerFactory::GetForProfile(profile),
      profile);
}

bool IpProtectionConfigProviderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Auth tokens will be requested soon after `Profile()` creation (after the
  // per-profile `NetworkContext()` gets created) so instantiate the
  // `IpProtectionConfigProvider()` so that it already exists by the time
  // that request is made.
  return true;
}
