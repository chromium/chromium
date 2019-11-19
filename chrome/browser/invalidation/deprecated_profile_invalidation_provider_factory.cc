// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/deprecated_profile_invalidation_provider_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/invalidator_storage.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/impl/ticl_invalidation_service.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if defined(OS_ANDROID)
#include "components/invalidation/impl/invalidation_service_android.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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

namespace {

#if !defined(OS_ANDROID)
void RequestProxyResolvingSocketFactoryOnUIThread(
    Profile* profile,
    base::WeakPtr<TiclInvalidationService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  if (!service)
    return;
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetNetworkContext();
  network_context->CreateProxyResolvingSocketFactory(std::move(receiver));
}

// A thread-safe wrapper to request a ProxyResolvingSocketFactory.
void RequestProxyResolvingSocketFactory(
    Profile* profile,
    base::WeakPtr<TiclInvalidationService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread, profile,
                     std::move(service), std::move(receiver)));
}

#endif

}  // namespace

// static
ProfileInvalidationProvider*
DeprecatedProfileInvalidationProviderFactory::GetForProfile(Profile* profile) {
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
DeprecatedProfileInvalidationProviderFactory*
DeprecatedProfileInvalidationProviderFactory::GetInstance() {
  return base::Singleton<DeprecatedProfileInvalidationProviderFactory>::get();
}

DeprecatedProfileInvalidationProviderFactory::
    DeprecatedProfileInvalidationProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "InvalidationService",
          BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
#endif
}

DeprecatedProfileInvalidationProviderFactory::
    ~DeprecatedProfileInvalidationProviderFactory() {}

void DeprecatedProfileInvalidationProviderFactory::RegisterTestingFactory(
    TestingFactory testing_factory) {
  testing_factory_ = std::move(testing_factory);
}

KeyedService*
DeprecatedProfileInvalidationProviderFactory::BuildServiceInstanceFor(
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

  std::unique_ptr<TiclInvalidationService> service =
      std::make_unique<TiclInvalidationService>(
          GetUserAgent(), identity_provider.get(),
          gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
          base::BindRepeating(&RequestProxyResolvingSocketFactory, profile),
          base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess(),
          content::GetNetworkConnectionTracker());
  service->Init(std::unique_ptr<syncer::InvalidationStateTracker>(
      new InvalidatorStorage(profile->GetPrefs())));

  return new ProfileInvalidationProvider(std::move(service),
                                         std::move(identity_provider), {});
#endif
}

void DeprecatedProfileInvalidationProviderFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ProfileInvalidationProvider::RegisterProfilePrefs(registry);
  InvalidatorStorage::RegisterProfilePrefs(registry);
}

}  // namespace invalidation
