// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_obsolete_profile_garbage_collector.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
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
constexpr std::string_view kObsoleteProfilesHistogramPrefix =
    "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles.";
constexpr std::string_view kDestroyedProfilesHistogramPrefix =
    "Crypto.UnexportableKeys.GarbageCollection.DestroyedProfiles.";

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
          base::BindOnce(
              [](std::unique_ptr<UnexportableKeyService>,
                 ServiceErrorOr<size_t> count_or_error) {
                if (count_or_error.has_value()) {
                  base::UmaHistogramCounts100(
                      base::StrCat({kDestroyedProfilesHistogramPrefix,
                                    "ObsoleteKeyDeletionCount"}),
                      *count_or_error);
                }
              },
              // Transfer ownership of `profile_service` to the callback to
              // ensure that the callback is run, even after `this` is
              // destroyed.
              std::move(profile_service)));
}

void UnexportableKeyObsoleteProfileGarbageCollector::
    OnProfileManagerDestroying() {
  // Invalidate all weak pointers to prevent any further calls to the profile
  // manager after it has been destroyed. This should only happen on shutdown.
  // The profile manager checks in its destructor that no observers are left,
  // thus it is not sufficient to just rely on the destructor of this class.
  // See https://crbug.com/485300762.
  weak_ptr_factory_.InvalidateWeakPtrs();
  profile_manager_observation_.Reset();
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

  std::vector<UnexportableKeyId>& key_ids = *key_ids_or_error;
  const size_t key_count = key_ids.size();
  base::UmaHistogramCounts100(
      base::StrCat({kObsoleteProfilesHistogramPrefix, "TotalKeyCount"}),
      key_count);

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
  size_t used_key_count = FilterUnexportableKeysByActiveApplicationTags(
      key_ids, *user_data_dir_service_, active_application_tag_prefixes);

  base::UmaHistogramCounts100(
      base::StrCat({kObsoleteProfilesHistogramPrefix, "UsedKeyCount"}),
      used_key_count);

  base::UmaHistogramCounts100(
      base::StrCat({kObsoleteProfilesHistogramPrefix, "ObsoleteKeyCount"}),
      key_ids.size());

  user_data_dir_service_->DeleteKeysSlowlyAsync(
      key_ids, BackgroundTaskPriority::kBestEffort,
      base::BindOnce([](ServiceErrorOr<size_t> result) {
        base::UmaHistogramCounts100(
            base::StrCat(
                {kObsoleteProfilesHistogramPrefix, "ObsoleteKeyDeletionCount"}),
            result.value_or(0));
      }));
}

}  // namespace unexportable_keys
