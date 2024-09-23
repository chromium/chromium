// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// static
KerberosCredentialsManager* KerberosCredentialsManagerFactory::GetExisting(
    content::BrowserContext* context) {
  return static_cast<KerberosCredentialsManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

// static
KerberosCredentialsManager* KerberosCredentialsManagerFactory::Get(
    content::BrowserContext* context) {
  return static_cast<KerberosCredentialsManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
KerberosCredentialsManagerFactory*
KerberosCredentialsManagerFactory::GetInstance() {
  static base::NoDestructor<KerberosCredentialsManagerFactory> instance;
  return instance.get();
}

KerberosCredentialsManagerFactory::KerberosCredentialsManagerFactory()
    : ProfileKeyedServiceFactory(
          /*name=*/"KerberosCredentialsManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

KerberosCredentialsManagerFactory::~KerberosCredentialsManagerFactory() =
    default;

bool KerberosCredentialsManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
KerberosCredentialsManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Verify that UserManager is initialized before calling IsPrimaryProfile.
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;

  Profile* const profile = Profile::FromBrowserContext(context);

  // We only create a service instance for primary profiles.
  if (!ProfileHelper::IsPrimaryProfile(profile))
    return nullptr;

  // Verify that this is not a testing profile.
  if (profile->AsTestingProfile())
    return nullptr;

  PrefService* local_state = g_browser_process->local_state();
  return std::make_unique<KerberosCredentialsManager>(local_state, profile);
}

}  // namespace ash
