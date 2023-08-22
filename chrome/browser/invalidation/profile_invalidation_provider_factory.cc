// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_content_client.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/files/file_path.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/device_identity/device_identity_provider.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "components/user_manager/user_manager.h"
#endif

namespace invalidation {
namespace {

std::unique_ptr<InvalidationService> CreateInvalidationServiceForSenderId(
    Profile* profile,
    IdentityProvider* identity_provider,
    const std::string& sender_id) {
  auto service = std::make_unique<FCMInvalidationService>(
      identity_provider,
      base::BindRepeating(
          &FCMNetworkHandler::Create,
          gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
          instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
              ->driver()),
      base::BindRepeating(
          &PerUserTopicSubscriptionManager::Create, identity_provider,
          profile->GetPrefs(),
          base::RetainedRef(profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess())),
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver(),
      profile->GetPrefs(), sender_id);
  service->Init();
  return service;
}

}  // namespace

// static
ProfileInvalidationProvider* ProfileInvalidationProviderFactory::GetForProfile(
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Using ProfileHelper::GetSigninProfile() here would lead to an infinite loop
  // when this method is called during the creation of the sign-in profile
  // itself. Using ProfileHelper::GetSigninProfileDir() is safe because it does
  // not try to access the sign-in profile.
  if (profile->GetPath() == ash::ProfileHelper::GetSigninProfileDir() ||
      (user_manager::UserManager::IsInitialized() &&
       user_manager::UserManager::Get()->IsLoggedInAsGuest())) {
    // The Chrome OS login and Chrome OS guest profiles do not have GAIA
    // credentials and do not support invalidation.
    return nullptr;
  }
#endif
  return static_cast<ProfileInvalidationProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileInvalidationProviderFactory*
ProfileInvalidationProviderFactory::GetInstance() {
  static base::NoDestructor<ProfileInvalidationProviderFactory> instance;
  return instance.get();
}

ProfileInvalidationProviderFactory::ProfileInvalidationProviderFactory()
    : ProfileKeyedServiceFactory(
          "InvalidationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
}

ProfileInvalidationProviderFactory::~ProfileInvalidationProviderFactory() =
    default;

void ProfileInvalidationProviderFactory::RegisterTestingFactory(
    TestingFactory testing_factory) {
  testing_factory_ = std::move(testing_factory);
}

std::unique_ptr<KeyedService>
ProfileInvalidationProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (testing_factory_)
    return testing_factory_.Run(context);

  std::unique_ptr<IdentityProvider> identity_provider;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsKioskApp() &&
      connector->IsDeviceEnterpriseManaged()) {
    identity_provider = std::make_unique<DeviceIdentityProvider>(
        DeviceOAuth2TokenServiceFactory::Get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  Profile* profile = Profile::FromBrowserContext(context);

  if (!identity_provider) {
    identity_provider = std::make_unique<ProfileIdentityProvider>(
        IdentityManagerFactory::GetForProfile(profile));
  }
  auto custom_sender_id_factory = base::BindRepeating(
      &CreateInvalidationServiceForSenderId, profile, identity_provider.get());
  return std::make_unique<ProfileInvalidationProvider>(
      std::move(identity_provider), std::move(custom_sender_id_factory));
}

void ProfileInvalidationProviderFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileInvalidationProvider::RegisterProfilePrefs(registry);
}

}  // namespace invalidation
