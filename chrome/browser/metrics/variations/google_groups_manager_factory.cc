// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/variations/service/google_groups_manager.h"

// static
GoogleGroupsManagerFactory*
GoogleGroupsManagerFactory::GetInstance() {
  static base::NoDestructor<GoogleGroupsManagerFactory> instance;
  return instance.get();
}

// static
GoogleGroupsManager*
GoogleGroupsManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<GoogleGroupsManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

GoogleGroupsManagerFactory::GoogleGroupsManagerFactory()
    : ProfileKeyedServiceFactory(
          "GoogleGroupsManager",
          // We only want instances of this service corresponding to regular
          // profiles, as those are the only ones that can have sync data to
          // copy from.
          // In the case of Incognito, the OTR profile will not have the service
          // created however the owning regular profile will be loaded and have
          // the service created.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

std::unique_ptr<KeyedService>
GoogleGroupsManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GoogleGroupsManager>(
      *g_browser_process->local_state(),
      // Profile paths are guaranteed to be UTF8-encoded.
      profile->GetPath().BaseName().AsUTF8Unsafe(), *profile->GetPrefs());
}

bool GoogleGroupsManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool GoogleGroupsManagerFactory::ServiceIsNULLWhileTesting() const {
  // Many unit test don't initialize g_browser_process->local_state(), so
  // disable this service in unit tests.
  return true;
}

void GoogleGroupsManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  GoogleGroupsManager::RegisterProfilePrefs(registry);
}
