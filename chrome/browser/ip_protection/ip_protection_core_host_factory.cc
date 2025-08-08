// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_core_host_factory.h"

#include "chrome/browser/ip_protection/ip_protection_core_host.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
IpProtectionCoreHost* IpProtectionCoreHostFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IpProtectionCoreHost*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IpProtectionCoreHostFactory* IpProtectionCoreHostFactory::GetInstance() {
  static base::NoDestructor<IpProtectionCoreHostFactory> instance;
  return instance.get();
}

// static
ProfileSelections IpProtectionCoreHostFactory::CreateProfileSelections() {
  if (!IpProtectionCoreHost::CanIpProtectionBeEnabled()) {
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

IpProtectionCoreHostFactory::IpProtectionCoreHostFactory()
    : ProfileKeyedServiceFactory("IpProtectionCoreHostFactory",
                                 CreateProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
}

IpProtectionCoreHostFactory::~IpProtectionCoreHostFactory() = default;

std::unique_ptr<KeyedService>
IpProtectionCoreHostFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<IpProtectionCoreHost>(
      IdentityManagerFactory::GetForProfile(profile),
      TrackingProtectionSettingsFactory::GetForProfile(profile),
      profile->GetPrefs(), profile);
}

bool IpProtectionCoreHostFactory::ServiceIsCreatedWithBrowserContext() const {
  // Auth tokens will be requested soon after `Profile()` creation (after the
  // per-profile `NetworkContext()` gets created) so instantiate the
  // `IpProtectionCoreHost()` so that it already exists by the time
  // that request is made.
  return true;
}
