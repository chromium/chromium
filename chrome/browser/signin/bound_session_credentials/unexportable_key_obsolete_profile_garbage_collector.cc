// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_obsolete_profile_garbage_collector.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/unexportable_key_service.h"

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

}  // namespace

UnexportableKeyObsoleteProfileGarbageCollector::
    UnexportableKeyObsoleteProfileGarbageCollector(
        ProfileManager* profile_manager)
    : user_data_dir_service_(
          UnexportableKeyServiceFactory::CreateForGarbageCollection(
              GetConfigForUserDataDir(profile_manager->user_data_dir()))) {
  CHECK(user_data_dir_service_);
  profile_manager_observation_.Observe(profile_manager);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UnexportableKeyObsoleteProfileGarbageCollector::
                         StartGarbageCollection,
                     weak_ptr_factory_.GetWeakPtr()),
      kGarbageCollectionDelay);
}

UnexportableKeyObsoleteProfileGarbageCollector::
    ~UnexportableKeyObsoleteProfileGarbageCollector() = default;

void UnexportableKeyObsoleteProfileGarbageCollector::
    OnProfileMarkedForPermanentDeletion(Profile* profile) {
  std::unique_ptr<UnexportableKeyService> profile_service =
      UnexportableKeyServiceFactory::CreateForGarbageCollection(
          GetConfigForProfilePath(profile->GetPath()));
  CHECK_DEREF(profile_service)
      .DeleteAllKeysSlowlyAsync(
          BackgroundTaskPriority::kBestEffort,
          // TODO(crbug.com/455538352): Add metrics.
          base::BindOnce([](std::unique_ptr<UnexportableKeyService>,
                            ServiceErrorOr<size_t>) {},
                         std::move(profile_service)));
}

void UnexportableKeyObsoleteProfileGarbageCollector::StartGarbageCollection() {
  user_data_dir_service_->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      BackgroundTaskPriority::kBestEffort,
      base::BindOnce(&UnexportableKeyObsoleteProfileGarbageCollector::
                         OnGetAllSigningKeysForGarbageCollection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UnexportableKeyObsoleteProfileGarbageCollector::
    OnGetAllSigningKeysForGarbageCollection(
        ServiceErrorOr<std::vector<UnexportableKeyId>> key_ids_or_error) {
  if (!key_ids_or_error.has_value() || key_ids_or_error->empty()) {
    return;
  }

  // Collect all profile paths that are known to the profile manager.
  std::vector<base::FilePath> profile_paths = base::ToVector(
      profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributes(),
      [](const ProfileAttributesEntry* entry) { return entry->GetPath(); });
  base::Extend(profile_paths, profile_manager()->GetLoadedProfiles(),
               [](const Profile* profile) { return profile->GetPath(); });

  // Create a set of all active application tag profile path level prefixes.
  const auto active_application_tag_prefixes = base::MakeFlatSet<std::string>(
      profile_paths, std::less<>(), [](const base::FilePath& profile_path) {
        return GetApplicationTag(GetConfigForProfilePath(profile_path));
      });

  // Filter `key_ids`, removing ids where we can't obtain the key's tag, or the
  // tag is known to be active.
  std::vector<UnexportableKeyId>& key_ids = *key_ids_or_error;
  std::erase_if(key_ids, [&](UnexportableKeyId key_id) -> bool {
    ASSIGN_OR_RETURN(std::string key_tag,
                     user_data_dir_service_->GetKeyTag(key_id),
                     [](auto) { return true; });
    // Since `active_application_tag_prefixes` is sorted, a possible prefix of
    // `key_tag` must come right before `key_tag` if it was in the set.
    auto it = active_application_tag_prefixes.upper_bound(key_tag);
    return it != active_application_tag_prefixes.begin() &&
           key_tag.starts_with(*std::prev(it));
  });

  // Schedule the rest for deletion.
  // TODO(crbug.com/455538832): Add a bulk deletion API to the service.
  for (UnexportableKeyId key_id : key_ids) {
    user_data_dir_service_->DeleteKeySlowlyAsync(
        // TODO(crbug.com/455538352): Add metrics.
        key_id, BackgroundTaskPriority::kBestEffort, base::DoNothing());
  }
}

}  // namespace unexportable_keys
