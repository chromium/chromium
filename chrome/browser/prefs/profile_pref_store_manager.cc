// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/profile_pref_store_manager.h"

#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/tracked/pref_hash_filter.h"
#include "services/preferences/tracked/tracked_persistent_pref_store_factory.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
// Forces a different registry key to be used for storing preference validation
// MACs. See |SetPreferenceValidationRegistryPathForTesting|.
const std::wstring* g_preference_validation_registry_path_for_testing = nullptr;
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

// Preference tracking and protection is not required on platforms where other
// apps do not have access to chrome's persistent storage.
const bool ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
    false;
#else
    true;
#endif

ProfilePrefStoreManager::ProfilePrefStoreManager(
    const base::FilePath& profile_path,
    const std::string& seed,
    const std::string& legacy_device_id)
    : profile_path_(profile_path),
      seed_(seed),
      legacy_device_id_(legacy_device_id) {}

ProfilePrefStoreManager::~ProfilePrefStoreManager() = default;

// static
void ProfilePrefStoreManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  PrefHashFilter::RegisterProfilePrefs(registry);
}

//  static
base::Time ProfilePrefStoreManager::GetResetTime(PrefService* pref_service) {
  return PrefHashFilter::GetResetTime(pref_service);
}

// static
void ProfilePrefStoreManager::ClearResetTime(PrefService* pref_service) {
  PrefHashFilter::ClearResetTime(pref_service);
}

#if BUILDFLAG(IS_WIN)
// static
void ProfilePrefStoreManager::SetPreferenceValidationRegistryPathForTesting(
    const std::wstring* path) {
  DCHECK(!path->empty());
  g_preference_validation_registry_path_for_testing = path;
}
#endif  // BUILDFLAG(IS_WIN)

PersistentPrefStore* ProfilePrefStoreManager::CreateProfilePrefStore(
    std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>
        tracking_configuration,
    size_t reporting_ids_count,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
        reset_on_load_observer,
    mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate) {
  if (!kPlatformSupportsPreferenceTracking) {
    return new JsonPrefStore(profile_path_.Append(chrome::kPreferencesFilename),
                             nullptr, io_task_runner);
  }
  return CreateTrackedPersistentPrefStore(
      CreateTrackedPrefStoreConfiguration(
          std::move(tracking_configuration), reporting_ids_count,
          std::move(reset_on_load_observer), std::move(validation_delegate)),
      io_task_runner);
}

bool ProfilePrefStoreManager::InitializePrefsFromMasterPrefs(
    std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>
        tracking_configuration,
    size_t reporting_ids_count,
    base::Value::Dict master_prefs) {
  // Create the profile directory if it doesn't exist yet (very possible on
  // first run).
  if (!base::CreateDirectory(profile_path_))
    return false;

  if (kPlatformSupportsPreferenceTracking) {
    InitializeMasterPrefsTracking(
        CreateTrackedPrefStoreConfiguration(std::move(tracking_configuration),
                                            reporting_ids_count, {},
                                            mojo::NullRemote()),
        master_prefs);
  }

  // This will write out to a single combined file which will be immediately
  // migrated to two files on load.
  JSONFileValueSerializer serializer(
      profile_path_.Append(chrome::kPreferencesFilename));

  // Call Serialize (which does IO) on the main thread, which would _normally_
  // be verboten. In this case however, we require this IO to synchronously
  // complete before Chrome can start (as master preferences seed the Local
  // State and Preferences files). This won't trip ThreadIORestrictions as they
  // won't have kicked in yet on the main thread.
  bool success = serializer.Serialize(master_prefs);

  return success;
}

prefs::mojom::TrackedPersistentPrefStoreConfigurationPtr
ProfilePrefStoreManager::CreateTrackedPrefStoreConfiguration(
    std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>
        tracking_configuration,
    size_t reporting_ids_count,
    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
        reset_on_load_observer,
    mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate) {
  return prefs::mojom::TrackedPersistentPrefStoreConfiguration::New(
      profile_path_.Append(chrome::kPreferencesFilename),
      profile_path_.Append(chrome::kSecurePreferencesFilename),
      std::move(tracking_configuration), reporting_ids_count, seed_,
      legacy_device_id_, "ChromeRegistryHashStoreValidationSeed",
#if BUILDFLAG(IS_WIN)
      base::AsString16(g_preference_validation_registry_path_for_testing
                           ? *g_preference_validation_registry_path_for_testing
                           : install_static::GetRegistryPath()),
#else
      std::u16string(),
#endif
      std::move(validation_delegate), std::move(reset_on_load_observer));
}
