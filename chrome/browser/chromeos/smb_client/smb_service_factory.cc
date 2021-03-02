// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"

#include <memory>

#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/authpolicy/authpolicy_credentials_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace smb_client {

namespace {

bool IsAllowedByPolicy(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kNetworkFileSharesAllowed);
}

bool DoesProfileHaveUser(const Profile* profile) {
  return chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
}

}  // namespace

SmbService* SmbServiceFactory::Get(content::BrowserContext* context) {
  return static_cast<SmbService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

SmbService* SmbServiceFactory::FindExisting(content::BrowserContext* context) {
  return static_cast<SmbService*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

SmbServiceFactory* SmbServiceFactory::GetInstance() {
  return base::Singleton<SmbServiceFactory>::get();
}

SmbServiceFactory::SmbServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SmbService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(file_system_provider::ServiceFactory::GetInstance());
  DependsOn(AuthPolicyCredentialsManagerFactory::GetInstance());
  DependsOn(KerberosCredentialsManagerFactory::GetInstance());
  DependsOn(file_manager::VolumeManagerFactory::GetInstance());
}

SmbServiceFactory::~SmbServiceFactory() {}

bool SmbServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* SmbServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Check if service is enabled by feature flag, via policy, and if profile has
  // a user. Lockscreen is the example of a profile that doesn't have a user -
  // in this case smb service is not needed.
  Profile* const profile = Profile::FromBrowserContext(context);
  bool service_should_run =
      IsAllowedByPolicy(profile) && DoesProfileHaveUser(profile);
  if (!service_should_run)
    return nullptr;
  return new SmbService(profile, std::make_unique<base::DefaultTickClock>());
}

content::BrowserContext* SmbServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

void SmbServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SmbService::RegisterProfilePrefs(registry);
}

}  // namespace smb_client
}  // namespace chromeos
