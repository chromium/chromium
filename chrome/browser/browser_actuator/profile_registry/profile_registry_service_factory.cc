// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/profile_registry/profile_registry_service_factory.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_actuator/local_actuation/local_actuation_names.h"
#include "chrome/browser/browser_actuator/profile_registry/profile_registry_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "components/browser_actuator/public/features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "crypto/apple/keychain_util.h"

namespace browser_actuator {

// static
ProfileRegistryService* ProfileRegistryServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<ProfileRegistryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
ProfileRegistryServiceFactory* ProfileRegistryServiceFactory::GetInstance() {
  static base::NoDestructor<ProfileRegistryServiceFactory> instance;
  return instance.get();
}

ProfileRegistryServiceFactory::ProfileRegistryServiceFactory()
    : ProfileKeyedServiceFactory("ProfileRegistryService",
                                 ProfileSelections::BuildForRegularProfile()) {}

ProfileRegistryServiceFactory::~ProfileRegistryServiceFactory() = default;

bool ProfileRegistryServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
ProfileRegistryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kBrowserActuatorProfileRegistry) ||
      !chrome::IsUsingDefaultDataDirectory().value_or(false) ||
      !crypto::apple::ExecutableHasKeychainAccessGroupEntitlement(
          kLocalActuationKeychainAccessGroup)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile ||
      (profile->GetPrefs() &&
       profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles))) {
    return nullptr;
  }

  ProfileManager* profile_manager =
      g_browser_process ? g_browser_process->profile_manager() : nullptr;
  if (profile_manager) {
    if (ProfileAttributesEntry* entry =
            profile_manager->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(profile->GetPath());
        entry && entry->IsEphemeral()) {
      return nullptr;
    }
  }

  return std::make_unique<ProfileRegistryService>();
}

}  // namespace browser_actuator
