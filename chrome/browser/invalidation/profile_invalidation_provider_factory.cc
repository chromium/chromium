// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/impl/ticl_invalidation_service.h"
#include "components/invalidation/impl/ticl_profile_settings_provider.h"
#include "components/invalidation/impl/ticl_settings_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "components/invalidation/impl/invalidation_service_android.h"
#else
#include "chrome/browser/signin/identity_manager_factory.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_identity_provider.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "components/user_manager/user_manager.h"
#endif

namespace invalidation {

// static
ProfileInvalidationProvider* ProfileInvalidationProviderFactory::GetForProfile(
    Profile* profile) {
#if defined(OS_CHROMEOS)
  // Using ProfileHelper::GetSigninProfile() here would lead to an infinite loop
  // when this method is called during the creation of the sign-in profile
  // itself. Using ProfileHelper::GetSigninProfileDir() is safe because it does
  // not try to access the sign-in profile.
  if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
      (user_manager::UserManager::IsInitialized() &&
       user_manager::UserManager::Get()->IsLoggedInAsGuest())) {
    // The Chrome OS login and Chrome OS guest profiles do not have GAIA
    // credentials and do not support invalidation.
    return NULL;
  }
#endif
  return static_cast<ProfileInvalidationProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileInvalidationProviderFactory*
ProfileInvalidationProviderFactory::GetInstance() {
  return base::Singleton<ProfileInvalidationProviderFactory>::get();
}

ProfileInvalidationProviderFactory::ProfileInvalidationProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "InvalidationService",
          BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
#endif
}

ProfileInvalidationProviderFactory::~ProfileInvalidationProviderFactory() =
    default;

void ProfileInvalidationProviderFactory::RegisterTestingFactory(
    TestingFactory testing_factory) {
  testing_factory_ = std::move(testing_factory);
}

KeyedService* ProfileInvalidationProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (testing_factory_)
    return testing_factory_.Run(context).release();

#if defined(OS_ANDROID)
  // Android does not need an IdentityProvider, because it gets the account
  // on the java side.
  auto service = std::make_unique<InvalidationServiceAndroid>();
  return new ProfileInvalidationProvider(std::move(service), nullptr);
#else

  std::unique_ptr<IdentityProvider> identity_provider;

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsKioskApp() &&
      connector->IsEnterpriseManaged()) {
    identity_provider.reset(new chromeos::DeviceIdentityProvider(
        chromeos::DeviceOAuth2TokenServiceFactory::Get()));
  }
#endif
  Profile* profile = Profile::FromBrowserContext(context);

  if (!identity_provider) {
    identity_provider.reset(new ProfileIdentityProvider(
        IdentityManagerFactory::GetForProfile(profile)));
  }
  std::unique_ptr<FCMInvalidationService> service =
      std::make_unique<FCMInvalidationService>(
          identity_provider.get(),
          gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
          instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
              ->driver(),
          profile->GetPrefs(),
          base::BindRepeating(data_decoder::SafeJsonParser::Parse,
                              content::ServiceManagerConnection::GetForProcess()
                                  ->GetConnector()),
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess()
              .get());
  service->Init();
  return new ProfileInvalidationProvider(std::move(service),
                                         std::move(identity_provider));
#endif
}

}  // namespace invalidation
