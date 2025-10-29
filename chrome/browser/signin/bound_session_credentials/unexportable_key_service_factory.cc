// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/chrome_version.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

// Returns the configuration for the `UnexportableKeyProvider`.
crypto::UnexportableKeyProvider::Config GetConfig() {
  return {
#if BUILDFLAG(IS_MAC)
      .keychain_access_group = MAC_TEAM_IDENTIFIER_STRING
      "." MAC_BUNDLE_IDENTIFIER_STRING ".unexportable-keys",
#endif  // BUILDFLAG(IS_MAC)
  };
}

// Returns a newly created task manager instance, or nullptr if unexportable
// keys are not available.
std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>
CreateTaskManagerInstance() {
  return unexportable_keys::UnexportableKeyServiceImpl::
                 IsUnexportableKeyProviderSupported(GetConfig())
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

// Creates an `UnexportableKeyServiceImpl` instance.
std::unique_ptr<unexportable_keys::UnexportableKeyService>
CreateUnexportableKeyServiceImpl() {
  return std::make_unique<unexportable_keys::UnexportableKeyServiceImpl>(
      CHECK_DEREF(GetSharedTaskManagerInstance()), GetConfig());
}

// Manages `UnexportableKeyService` instances for different purposes.
class UnexportableKeyServiceManager : public KeyedService {
 public:
  using ServiceFactory = UnexportableKeyServiceFactory::ServiceFactory;

  explicit UnexportableKeyServiceManager(ServiceFactory service_factory)
      : service_factory_(std::move(service_factory)) {}

  unexportable_keys::UnexportableKeyService* GetOrCreateService(
      UnexportableKeyServiceFactory::KeyPurpose purpose) {
    const auto& [_, service] = *services_.lazy_emplace(
        purpose,
        [&](const auto& ctor) { ctor(purpose, service_factory_.Run()); });
    return service.get();
  }

 private:
  ServiceFactory service_factory_;

  // Map to hold individual `UnexportableKeyService` instances, keyed by
  // `KeyPurpose`.
  absl::flat_hash_map<
      UnexportableKeyServiceFactory::KeyPurpose,
      std::unique_ptr<unexportable_keys::UnexportableKeyService>>
      services_;
};

}  // namespace

// static
unexportable_keys::UnexportableKeyService*
UnexportableKeyServiceFactory::GetForProfileAndPurpose(Profile* profile,
                                                       KeyPurpose purpose) {
  auto* manager = static_cast<UnexportableKeyServiceManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
  return manager != nullptr ? manager->GetOrCreateService(purpose) : nullptr;
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
      service_factory_for_testing_.is_null()
          ? base::BindRepeating(&CreateUnexportableKeyServiceImpl)
          : service_factory_for_testing_);
}
