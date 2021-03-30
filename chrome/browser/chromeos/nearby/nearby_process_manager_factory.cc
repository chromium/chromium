// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/nearby_process_manager_factory.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/nearby/nearby_connections_dependencies_provider_factory.h"
#include "chrome/browser/chromeos/nearby/nearby_process_manager_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace nearby {
namespace {

bool g_bypass_primary_user_check_for_testing = false;

}  // namespace

// static
NearbyProcessManager* NearbyProcessManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NearbyProcessManager*>(
      NearbyProcessManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
bool NearbyProcessManagerFactory::CanBeLaunchedForProfile(Profile* profile) {
  // Guest/incognito profiles cannot use Phone Hub.
  if (profile->IsOffTheRecord())
    return false;

  // Likewise, kiosk users are ineligible.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp())
    return false;

  return ProfileHelper::IsPrimaryProfile(profile);
}

// static
NearbyProcessManagerFactory* NearbyProcessManagerFactory::GetInstance() {
  return base::Singleton<NearbyProcessManagerFactory>::get();
}

// static
void NearbyProcessManagerFactory::SetBypassPrimaryUserCheckForTesting(
    bool bypass_primary_user_check_for_testing) {
  g_bypass_primary_user_check_for_testing =
      bypass_primary_user_check_for_testing;
}

NearbyProcessManagerFactory::NearbyProcessManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "NearbyProcessManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NearbyConnectionsDependenciesProviderFactory::GetInstance());
}

NearbyProcessManagerFactory::~NearbyProcessManagerFactory() = default;

KeyedService* NearbyProcessManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // The service is meant to be a singleton, since multiple simultaneous process
  // managers could interfere with each other. Provide access only to the
  // primary user.
  if (CanBeLaunchedForProfile(profile) ||
      g_bypass_primary_user_check_for_testing) {
    return NearbyProcessManagerImpl::Factory::Create(
               NearbyConnectionsDependenciesProviderFactory::GetForProfile(
                   profile))
        .release();
  }

  return nullptr;
}

bool NearbyProcessManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace nearby
}  // namespace chromeos
