// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_service_factory.h"

#include <memory>

#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace smb_client {

namespace {

bool IsAllowedByPolicy(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kNetworkFileSharesAllowed);
}

bool DoesProfileHaveUser(const Profile* profile) {
  return ProfileHelper::Get()->GetUserByProfile(profile);
}

}  // namespace

SmbService* SmbServiceFactory::Get(content::BrowserContext* context) {
  return static_cast<SmbService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

SmbService* SmbServiceFactory::FindExisting(content::BrowserContext* context) {
  return static_cast<SmbService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

SmbServiceFactory* SmbServiceFactory::GetInstance() {
  static base::NoDestructor<SmbServiceFactory> instance;
  return instance.get();
}

SmbServiceFactory::SmbServiceFactory()
    : ProfileKeyedServiceFactory(
          /*name=*/"SmbService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(file_system_provider::ServiceFactory::GetInstance());
  DependsOn(KerberosCredentialsManagerFactory::GetInstance());
  DependsOn(file_manager::VolumeManagerFactory::GetInstance());
}

SmbServiceFactory::~SmbServiceFactory() = default;

bool SmbServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
SmbServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Check if service is enabled by feature flag, via policy, and if profile has
  // a user. Lock screen is the example of a profile that doesn't have a user -
  // in this case smb service is not needed.
  Profile* const profile = Profile::FromBrowserContext(context);
  bool service_should_run =
      IsAllowedByPolicy(profile) && DoesProfileHaveUser(profile);
  if (!service_should_run)
    return nullptr;
  return std::make_unique<SmbService>(
      profile, std::make_unique<base::DefaultTickClock>());
}

void SmbServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SmbService::RegisterProfilePrefs(registry);
}

}  // namespace smb_client
}  // namespace ash
