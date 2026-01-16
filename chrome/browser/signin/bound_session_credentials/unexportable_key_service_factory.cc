// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/mojom/unexportable_key_service_proxy_impl.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/hash.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

// Returns a newly created task manager instance, or nullptr if unexportable
// keys are not available.
std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>
CreateTaskManagerInstance() {
  return unexportable_keys::UnexportableKeyServiceImpl::
                 IsUnexportableKeyProviderSupported(
                     unexportable_keys::GetDefaultConfig())
             ? std::make_unique<unexportable_keys::UnexportableKeyTaskManager>()
             : nullptr;
}

// Returns an `UnexportableKeyTaskManager` instance that is shared across all
// profiles, or nullptr if unexportable keys are not available. This function
// caches availability, so any flags that may change it must be set before the
// first call.
//
// Note: this instance is currently accessible only to
// `UnexportableKeyServiceFactory`. The getter can be moved to some common place
// if there is a need.
unexportable_keys::UnexportableKeyTaskManager* GetSharedTaskManagerInstance() {
  static base::NoDestructor<
      std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>>
      instance(CreateTaskManagerInstance());
  return instance->get();
}

// Returns a newly created `UnexportableKeyService` instance, or nullptr if
// unexportable keys are not available.
std::unique_ptr<unexportable_keys::UnexportableKeyService> CreateService(
    const UnexportableKeyServiceFactory::ServiceFactory& service_factory,
    unexportable_keys::BackgroundTaskOrigin task_origin,
    crypto::UnexportableKeyProvider::Config config) {
  if (!service_factory.is_null()) {
    return service_factory.Run(std::move(config));
  }

  if (unexportable_keys::UnexportableKeyTaskManager* task_manager =
          GetSharedTaskManagerInstance()) {
    return std::make_unique<unexportable_keys::UnexportableKeyServiceImpl>(
        *task_manager, task_origin, std::move(config));
  }

  return nullptr;
}

unexportable_keys::BackgroundTaskOrigin GetTaskOriginForPurpose(
    unexportable_keys::KeyPurpose purpose) {
  using FromEnum = unexportable_keys::KeyPurpose;
  using ToEnum = unexportable_keys::BackgroundTaskOrigin;
  switch (purpose) {
    case FromEnum::kRefreshTokenBinding:
      return ToEnum::kRefreshTokenBinding;
    case FromEnum::kDeviceBoundSessionCredentials:
      return ToEnum::kDeviceBoundSessionCredentials;
    case FromEnum::kDeviceBoundSessionCredentialsPrototype:
      return ToEnum::kDeviceBoundSessionCredentialsPrototype;
  }
}

// Manages `UnexportableKeyService` instances for different purposes.
class UnexportableKeyServiceManager : public KeyedService {
 public:
  using ServiceFactory = UnexportableKeyServiceFactory::ServiceFactory;

  explicit UnexportableKeyServiceManager(
      const Profile& profile,
      ServiceFactory service_factory_for_testing)
      : profile_(profile),
        service_factory_for_testing_(std::move(service_factory_for_testing)) {}

  unexportable_keys::UnexportableKeyService*
  GetOrCreateServiceForPathAndPurpose(
      const base::FilePath& relative_partition_path,
      unexportable_keys::KeyPurpose purpose) {
    auto& service = services_[{relative_partition_path, purpose}];
    if (service == nullptr) {
      service = CreateService(
          service_factory_for_testing_, GetTaskOriginForPurpose(purpose),
          unexportable_keys::GetConfigForStoragePartitionPathAndPurpose(
              *profile_, relative_partition_path, purpose));
    }
    return service.get();
  }

  unexportable_keys::UnexportableKeyServiceProxyImpl*
  GetOrCreateMojoProxyServiceWithReceiver(
      const base::FilePath& relative_partition_path,
      unexportable_keys::KeyPurpose purpose,
      mojo::PendingReceiver<unexportable_keys::mojom::UnexportableKeyService>
          receiver) {
    unexportable_keys::UnexportableKeyService* uks =
        GetOrCreateServiceForPathAndPurpose(relative_partition_path, purpose);
    CHECK(uks);

    return proxy_services_
        .insert_or_assign(
            {relative_partition_path, purpose},
            std::make_unique<
                unexportable_keys::UnexportableKeyServiceProxyImpl>(
                uks, std::move(receiver)))
        .first->second.get();
  }

 private:
  using PathAndPurpose =
      std::pair<base::FilePath, unexportable_keys::KeyPurpose>;

  raw_ref<const Profile> profile_;
  ServiceFactory service_factory_for_testing_;

  // Map to hold individual `UnexportableKeyService` instances, keyed by
  // `relative_partition_path` and `KeyPurpose`.
  absl::flat_hash_map<
      PathAndPurpose,
      std::unique_ptr<unexportable_keys::UnexportableKeyService>>
      services_;

  // Map to hold individual `UnexportableKeyServiceProxyImpl` instances, keyed
  // by `relative_partition_path` and `KeyPurpose`.
  absl::flat_hash_map<
      PathAndPurpose,
      std::unique_ptr<unexportable_keys::UnexportableKeyServiceProxyImpl>>
      proxy_services_;
};

}  // namespace

// static
std::unique_ptr<unexportable_keys::UnexportableKeyService>
UnexportableKeyServiceFactory::CreateForGarbageCollection(
    crypto::UnexportableKeyProvider::Config config) {
  return CreateService(
      GetInstance()->service_factory_for_testing_,
      unexportable_keys::BackgroundTaskOrigin::kOrphanedKeyGarbageCollection,
      std::move(config));
}

// static
unexportable_keys::UnexportableKeyService*
UnexportableKeyServiceFactory::GetForProfileAndPurpose(
    Profile* profile,
    unexportable_keys::KeyPurpose purpose) {
  return GetForStoragePartitionPathAndPurpose(profile, base::FilePath(),
                                              purpose);
}

// static
unexportable_keys::UnexportableKeyService*
UnexportableKeyServiceFactory::GetForStoragePartitionPathAndPurpose(
    Profile* profile,
    const base::FilePath& relative_partition_path,
    unexportable_keys::KeyPurpose purpose) {
  auto* manager = static_cast<UnexportableKeyServiceManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
  return manager != nullptr ? manager->GetOrCreateServiceForPathAndPurpose(
                                  relative_partition_path, purpose)
                            : nullptr;
}

// static
unexportable_keys::UnexportableKeyServiceProxyImpl*
UnexportableKeyServiceFactory::
    RecreateMojoProxyForStoragePartitionPathAndPurposeWithReceiver(
        Profile* profile,
        const base::FilePath& relative_partition_path,
        unexportable_keys::KeyPurpose purpose,
        mojo::PendingReceiver<unexportable_keys::mojom::UnexportableKeyService>
            receiver) {
  auto* manager = static_cast<UnexportableKeyServiceManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
  return manager != nullptr
             ? manager->GetOrCreateMojoProxyServiceWithReceiver(
                   relative_partition_path, purpose, std::move(receiver))
             : nullptr;
}

// static
UnexportableKeyServiceFactory* UnexportableKeyServiceFactory::GetInstance() {
  static base::NoDestructor<UnexportableKeyServiceFactory> instance;
  return instance.get();
}

UnexportableKeyServiceFactory::UnexportableKeyServiceFactory()
    : ProfileKeyedServiceFactory(
          "UnexportableKeyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // Only an OTR profile is used for browsing in the Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

UnexportableKeyServiceFactory::~UnexportableKeyServiceFactory() = default;

void UnexportableKeyServiceFactory::SetServiceFactoryForTesting(
    ServiceFactory factory) {
  service_factory_for_testing_ = std::move(factory);
}

// ProfileKeyedServiceFactory:
std::unique_ptr<KeyedService>
UnexportableKeyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  unexportable_keys::UnexportableKeyTaskManager* task_manager =
      GetSharedTaskManagerInstance();
  if (service_factory_for_testing_.is_null() && task_manager == nullptr) {
    // Do not create a service if the platform doesn't support unexportable
    // signing keys.
    return nullptr;
  }

  return std::make_unique<UnexportableKeyServiceManager>(
      *Profile::FromBrowserContext(context), service_factory_for_testing_);
}
