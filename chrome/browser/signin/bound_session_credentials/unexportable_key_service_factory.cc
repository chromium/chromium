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
#include "chrome/common/chrome_version.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/mojom/unexportable_key_service_proxy_impl.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/hash.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

#if BUILDFLAG(IS_MAC)
// Returns the first 64 bits of the SHA-256 hash of the given `data` as a
// lowercase hex string.
std::string HexEncodeLowerSha64(base::span<const uint8_t> data) {
  return base::HexEncodeLower(
      base::as_byte_span(crypto::hash::Sha256(data)).first<8>());
}

std::string HexEncodeLowerSha64(std::string_view data) {
  return HexEncodeLowerSha64(base::as_byte_span(data));
}

std::string_view PurposeToString(
    UnexportableKeyServiceFactory::KeyPurpose purpose) {
  switch (purpose) {
    case UnexportableKeyServiceFactory::KeyPurpose::kRefreshTokenBinding:
      return "lst";
    case UnexportableKeyServiceFactory::KeyPurpose::
        kDeviceBoundSessionCredentials:
      return "dbsc";
    case UnexportableKeyServiceFactory::KeyPurpose::
        kDeviceBoundSessionCredentialsPrototype:
      return "dbsc-prototype";
  }

  NOTREACHED();
}

// Returns an application tag for the `UnexportableKeyProvider` for the given
// `profile` and `purpose`. This tag is used on macOS to group related keys in
// the Keychain so they can be queried and deleted together.
//
// The tag is constructed to ensure keys are uniquely scoped to a specific
// profile and use case, which is critical for cleaning up orphaned keys when a
// profile is deleted or an incognito session ends. It is composed of:
// - The bundle and team identifiers to scope it to the application.
// - A hash of the current profile's user data directory.
// - The profile's name to uniquely identify the profile.
// - A hash of the profile's creation time to distinguish OTR profiles that have
//   dedicated cleanup logic.
// - A string representing the key's `purpose` (e.g., "dbsc", "lst").
//
// This allows for safe, bulk deletion of keys that are no longer in use without
// affecting keys from other profiles or for other purposes.
std::string GetKeyChainApplicationTag(
    const Profile& profile,
    UnexportableKeyServiceFactory::KeyPurpose purpose) {
  const std::string profile_dirname_hash =
      HexEncodeLowerSha64(profile.GetPath().DirName().value());
  const std::string profile_basename = profile.GetBaseName().value();
  const std::string profile_creation_time_hash =
      HexEncodeLowerSha64(base::byte_span_from_ref(
          profile.GetCreationTime().InMillisecondsSinceUnixEpoch()));
  return base::JoinString(
      {
          UnexportableKeyServiceFactory::GetKeychainAccessGroup(),
          profile_dirname_hash,
          profile_basename,
          profile_creation_time_hash,
          PurposeToString(purpose),
      },
      ".");
}
#endif  // BUILDFLAG(IS_MAC)

// Returns the configuration for the `UnexportableKeyProvider` for the given
// `profile` and `purpose`.
crypto::UnexportableKeyProvider::Config GetConfig(
    const Profile& profile,
    UnexportableKeyServiceFactory::KeyPurpose purpose) {
  return {
#if BUILDFLAG(IS_MAC)
      .keychain_access_group =
          UnexportableKeyServiceFactory::GetKeychainAccessGroup(),
      .application_tag = GetKeyChainApplicationTag(profile, purpose),
#endif  // BUILDFLAG(IS_MAC)
  };
}

// Returns a newly created task manager instance, or nullptr if unexportable
// keys are not available.
std::unique_ptr<unexportable_keys::UnexportableKeyTaskManager>
CreateTaskManagerInstance() {
  return unexportable_keys::UnexportableKeyServiceImpl::
                 IsUnexportableKeyProviderSupported({
#if BUILDFLAG(IS_MAC)
                     .keychain_access_group = UnexportableKeyServiceFactory::
                         GetKeychainAccessGroup(),
#endif  // BUILDFLAG(IS_MAC)
                 })
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

// Manages `UnexportableKeyService` instances for different purposes.
class UnexportableKeyServiceManager : public KeyedService {
 public:
  using ServiceFactory = UnexportableKeyServiceFactory::ServiceFactory;

  explicit UnexportableKeyServiceManager(
      const Profile& profile,
      ServiceFactory service_factory_for_testing)
      : profile_(profile),
        service_factory_for_testing_(std::move(service_factory_for_testing)) {}

  unexportable_keys::UnexportableKeyService* GetOrCreateService(
      UnexportableKeyServiceFactory::KeyPurpose purpose) {
    const auto& [_, service] =
        *services_.lazy_emplace(purpose, [&](const auto& ctor) {
          ctor(purpose, CreateServiceForPurpose(purpose));
        });
    return service.get();
  }

  unexportable_keys::UnexportableKeyServiceProxyImpl*
  GetOrCreateMojoProxyServiceWithReceiver(
      UnexportableKeyServiceFactory::KeyPurpose purpose,
      mojo::PendingReceiver<unexportable_keys::mojom::UnexportableKeyService>
          receiver) {
    unexportable_keys::UnexportableKeyService* uks =
        GetOrCreateService(purpose);
    CHECK(uks);

    auto new_service =
        std::make_unique<unexportable_keys::UnexportableKeyServiceProxyImpl>(
            uks, std::move(receiver));

    auto [it, inserted] =
        proxy_services_.insert_or_assign(purpose, std::move(new_service));

    return it->second.get();
  }

 private:
  std::unique_ptr<unexportable_keys::UnexportableKeyService>
  CreateServiceForPurpose(UnexportableKeyServiceFactory::KeyPurpose purpose) {
    return service_factory_for_testing_.is_null()
               ? std::make_unique<
                     unexportable_keys::UnexportableKeyServiceImpl>(
                     CHECK_DEREF(GetSharedTaskManagerInstance()),
                     GetConfig(*profile_, purpose))
               : service_factory_for_testing_.Run(
                     GetConfig(*profile_, purpose));
  }

  raw_ref<const Profile> profile_;
  ServiceFactory service_factory_for_testing_;

  // Map to hold individual `UnexportableKeyService` instances, keyed by
  // `KeyPurpose`.
  absl::flat_hash_map<
      UnexportableKeyServiceFactory::KeyPurpose,
      std::unique_ptr<unexportable_keys::UnexportableKeyService>>
      services_;

  // Map to hold individual `UnexportableKeyServiceProxyImpl` instances, keyed
  // by `KeyPurpose`.
  absl::flat_hash_map<
      UnexportableKeyServiceFactory::KeyPurpose,
      std::unique_ptr<unexportable_keys::UnexportableKeyServiceProxyImpl>>
      proxy_services_;
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
unexportable_keys::UnexportableKeyServiceProxyImpl*
UnexportableKeyServiceFactory::
    RecreateMojoProxyForProfileAndPurposeWithReceiver(
        Profile* profile,
        KeyPurpose purpose,
        mojo::PendingReceiver<unexportable_keys::mojom::UnexportableKeyService>
            receiver) {
  auto* manager = static_cast<UnexportableKeyServiceManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
  return manager != nullptr ? manager->GetOrCreateMojoProxyServiceWithReceiver(
                                  purpose, std::move(receiver))
                            : nullptr;
}

// static
UnexportableKeyServiceFactory* UnexportableKeyServiceFactory::GetInstance() {
  static base::NoDestructor<UnexportableKeyServiceFactory> instance;
  return instance.get();
}

#if BUILDFLAG(IS_MAC)
// static
std::string UnexportableKeyServiceFactory::GetKeychainAccessGroup() {
  return MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING
                                    ".unexportable-keys";
}
#endif  // BUILDFLAG(IS_MAC)

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
