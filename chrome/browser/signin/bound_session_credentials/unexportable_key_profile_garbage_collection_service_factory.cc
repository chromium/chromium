// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_profile_garbage_collection_service_factory.h"

#include <cstddef>
#include <memory>

#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"

namespace unexportable_keys {
namespace {

constexpr base::TimeDelta kGarbageCollectionDelay = base::Minutes(2);

std::string GetApplicationTag(crypto::UnexportableKeyProvider::Config config) {
#if BUILDFLAG(IS_MAC)
  return std::move(config.application_tag);
#else
  return std::string();
#endif  // BUILDFLAG(IS_MAC)
}

class OriginalProfileGarbageCollectionService : public KeyedService {
 public:
  explicit OriginalProfileGarbageCollectionService(Profile& profile)
      : profile_(profile) {
    CHECK(!profile_->IsOffTheRecord());
    CHECK(service_);
    // Schedule a task for original profiles to obtain all keys that were
    // created for this profile in the past, including all OTR profiles.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &OriginalProfileGarbageCollectionService::StartGarbageCollection,
            weak_ptr_factory_.GetWeakPtr()),
        kGarbageCollectionDelay);
  }

 private:
  void StartGarbageCollection() {
    service_->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
        BackgroundTaskPriority::kBestEffort,
        base::BindOnce(&OriginalProfileGarbageCollectionService::
                           OnGetAllSigningKeysForGarbageCollection,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetAllSigningKeysForGarbageCollection(
      ServiceErrorOr<std::vector<UnexportableKeyId>> key_ids_or_error) {
    if (!key_ids_or_error.has_value() || key_ids_or_error->empty()) {
      return;
    }

    // Start by creating a set of application_tag prefixes belonging to all tags
    // that correspond to still active profiles (original and all current OTR
    // profiles).
    std::vector<const Profile*> active_profiles = {&*profile_};
    base::Extend(active_profiles, profile_->GetAllOffTheRecordProfiles());
    const auto active_application_tag_prefixes = base::MakeFlatSet<std::string>(
        active_profiles, std::less<>(), [](const Profile* profile) {
          return GetApplicationTag(GetConfigForProfile(*profile));
        });

    // Remove all key ids where no tag could be obtained, or the prefix is still
    // active.
    std::vector<UnexportableKeyId>& key_ids = *key_ids_or_error;
    std::erase_if(key_ids, [&](UnexportableKeyId key_id) -> bool {
      ASSIGN_OR_RETURN(std::string key_tag, service_->GetKeyTag(key_id),
                       [](auto) { return true; });
      // Since `active_application_tag_prefixes` is sorted, a possible prefix of
      // `key_tag` must come right before `key_tag` if it was in the set.
      // TODO(crbug.com/455538832): This logic is shared between the garbage
      // collection classes. Move it to a shared location and add tests.
      auto it = active_application_tag_prefixes.upper_bound(key_tag);
      return it != active_application_tag_prefixes.begin() &&
             key_tag.starts_with(*std::prev(it));
    });

    // Schedule the rest for deletion.
    // TODO(crbug.com/455538832): Add a bulk deletion API to the service.
    for (UnexportableKeyId key_id : key_ids) {
      service_->DeleteKeySlowlyAsync(
          // TODO(crbug.com/455538352): Add metrics.
          key_id, BackgroundTaskPriority::kBestEffort, base::DoNothing());
    }
  }

  const raw_ref<Profile> profile_;
  std::unique_ptr<UnexportableKeyService> service_ =
      UnexportableKeyServiceFactory::CreateForGarbageCollection(
          GetConfigForProfilePath(profile_->GetPath()));
  base::WeakPtrFactory<OriginalProfileGarbageCollectionService>
      weak_ptr_factory_{this};
};

class OffTheRecordGarbageCollectionService : public KeyedService {
 public:
  explicit OffTheRecordGarbageCollectionService(const Profile& profile)
      : service_(UnexportableKeyServiceFactory::CreateForGarbageCollection(
            GetConfigForProfile(profile))) {
    CHECK(profile.IsOffTheRecord());
    CHECK(service_);
  }

 private:
  // KeyedService:
  void Shutdown() override {
    // Delete all keys for OTR profiles.
    service_->DeleteAllKeysSlowlyAsync(
        BackgroundTaskPriority::kBestEffort,
        base::BindOnce(
            [](std::unique_ptr<UnexportableKeyService>,
               ServiceErrorOr<size_t>) {
              // TODO(crbug.com/455538352): Add metrics.
            },
            std::move(service_)));
  }

  std::unique_ptr<UnexportableKeyService> service_;
};

}  // namespace

// static
UnexportableKeyProfileGarbageCollectionServiceFactory*
UnexportableKeyProfileGarbageCollectionServiceFactory::GetInstance() {
  static base::NoDestructor<
      UnexportableKeyProfileGarbageCollectionServiceFactory>
      instance;
  return instance.get();
}

KeyedService* UnexportableKeyProfileGarbageCollectionServiceFactory::
    GetServiceForBrowserContext(content::BrowserContext* context) const {
  return GetInstance()->ProfileKeyedServiceFactory::GetServiceForBrowserContext(
      context, /*create=*/true);
}

UnexportableKeyProfileGarbageCollectionServiceFactory::
    UnexportableKeyProfileGarbageCollectionServiceFactory()
    : ProfileKeyedServiceFactory(
          "UnexportableKeyProfileGarbageCollectionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(UnexportableKeyServiceFactory::GetInstance());
}

bool UnexportableKeyProfileGarbageCollectionServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
UnexportableKeyProfileGarbageCollectionServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kUnexportableKeyDeletion)) {
    return nullptr;
  }

  if (!UnexportableKeyServiceImpl::IsStatefulUnexportableKeyProviderSupported(
          GetDefaultConfig())) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (CHECK_DEREF(profile).IsOffTheRecord()) {
    return std::make_unique<OffTheRecordGarbageCollectionService>(*profile);
  }

  return std::make_unique<OriginalProfileGarbageCollectionService>(*profile);
}

}  // namespace unexportable_keys
