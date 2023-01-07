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
  // Instance ID is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<InstanceIDProfileService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstanceIDProfileServiceFactory*
InstanceIDProfileServiceFactory::GetInstance() {
  return base::Singleton<InstanceIDProfileServiceFactory>::get();
}

InstanceIDProfileServiceFactory::InstanceIDProfileServiceFactory()
    : ProfileKeyedServiceFactory(
          "InstanceIDProfileService",
          ProfileSelections::BuildForRegularAndIncognito()) {
  // GCM is needed for device ID.
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
}

InstanceIDProfileServiceFactory::~InstanceIDProfileServiceFactory() {
}

KeyedService* InstanceIDProfileServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new InstanceIDProfileService(
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
      profile->IsOffTheRecord());
}

}  // namespace instance_id
