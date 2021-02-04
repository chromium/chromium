// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

namespace {

Profile* GetPrimaryProfileFromContext(content::BrowserContext* context) {
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;
  // Get original profile, so it gets primary profile faster if context is
  // incognito profile.
  Profile* profile = Profile::FromBrowserContext(context)->GetOriginalProfile();
  if (!ProfileHelper::IsPrimaryProfile(profile)) {
    const auto* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    if (!primary_user)
      return nullptr;
    // Get primary profile from primary user. Note that it only gets primary
    // profile if it is fully created.
    profile = chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  }
  return profile;
}

}  // namespace

// static
KerberosCredentialsManager* KerberosCredentialsManagerFactory::GetExisting(
    content::BrowserContext* context) {
  Profile* const primary_profile = GetPrimaryProfileFromContext(context);
  if (!primary_profile)
    return nullptr;
  return static_cast<KerberosCredentialsManager*>(
      GetInstance()->GetServiceForBrowserContext(primary_profile, false));
}

// static
KerberosCredentialsManager* KerberosCredentialsManagerFactory::Get(
    content::BrowserContext* context) {
  Profile* const primary_profile = GetPrimaryProfileFromContext(context);
  if (!primary_profile)
    return nullptr;
  return static_cast<KerberosCredentialsManager*>(
      GetInstance()->GetServiceForBrowserContext(primary_profile, true));
}

// static
KerberosCredentialsManagerFactory*
KerberosCredentialsManagerFactory::GetInstance() {
  return base::Singleton<KerberosCredentialsManagerFactory>::get();
}

KerberosCredentialsManagerFactory::KerberosCredentialsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "KerberosCredentialsManager",
          BrowserContextDependencyManager::GetInstance()),
      service_instance_created_(false) {}

KerberosCredentialsManagerFactory::~KerberosCredentialsManagerFactory() =
    default;

bool KerberosCredentialsManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* KerberosCredentialsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);

  // Verify that UserManager is initialized before calling IsPrimaryProfile.
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;

  // Verify that we create instance for a primary profile.
  if (!ProfileHelper::IsPrimaryProfile(profile))
    return nullptr;

  // Verify that this is not a testing profile.
  if (profile->AsTestingProfile())
    return nullptr;

  // Make sure one and only one instance is ever created.
  if (service_instance_created_)
    return nullptr;
  service_instance_created_ = true;

  PrefService* local_state = g_browser_process->local_state();
  return new KerberosCredentialsManager(local_state, profile);
}

}  // namespace chromeos
