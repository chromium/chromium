// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"

#include <memory>

#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"

namespace instance_id {

// static
InstanceIDProfileService* InstanceIDProfileServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // On desktop, the guest profile is actually the primary OTR profile of
  // the "regular" guest profile.  The regular guest profile is never used
  // directly by users.  Also, user are not able to create child OTR profiles
  // from guest profiles, the menu item "New incognito window" is not
  // available.  So, if this is a guest session, allow it only if it is a
  // child OTR profile as well.
  bool is_profile_supported =
      !Profile::FromBrowserContext(profile)->IsIncognitoProfile();
#else
  bool is_profile_supported = !profile->IsOffTheRecord();
#endif

  // Instance ID is not supported in incognito mode.
  if (!is_profile_supported) {
    return nullptr;
  }

  return static_cast<InstanceIDProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstanceIDProfileServiceFactory*
InstanceIDProfileServiceFactory::GetInstance() {
  static base::NoDestructor<InstanceIDProfileServiceFactory> instance;
  return instance.get();
}

InstanceIDProfileServiceFactory::InstanceIDProfileServiceFactory()
    : ProfileKeyedServiceFactory(
          "InstanceIDProfileService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  // GCM is needed for device ID.
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
}

InstanceIDProfileServiceFactory::~InstanceIDProfileServiceFactory() = default;

KeyedService* InstanceIDProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // On desktop, incognito profiles are checked with IsIncognitoProfile().
  // It's possible for non-incognito profiles to also be off-the-record.
  bool is_incognito = profile->IsIncognitoProfile();
#else
  bool is_incognito = profile->IsOffTheRecord();
#endif

  return new InstanceIDProfileService(
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
      is_incognito);
}

}  // namespace instance_id
