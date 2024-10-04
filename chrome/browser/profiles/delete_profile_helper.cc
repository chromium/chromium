// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/delete_profile_helper.h"

#include <memory>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Called after a deleted profile was checked and cleaned up.
void ProfileCleanedUp(base::Value profile_path_value) {
  ScopedListPrefUpdate deleted_profiles(g_browser_process->local_state(),
                                        prefs::kProfilesDeleted);
  deleted_profiles->EraseValue(profile_path_value);
}

// Helper function that deletes entries from the kProfilesLastActive pref list.
// It is called when every ephemeral profile is handled.
void RemoveFromLastActiveProfilesPrefList(const base::FilePath& path) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  ScopedListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::Value::List& profile_list = update.Get();
  base::Value entry_value = base::Value(path.BaseName().AsUTF8Unsafe());
  profile_list.EraseValue(entry_value);
}

bool IsRegisteredAsEphemeral(ProfileAttributesStorage* storage,
                             const base::FilePath& profile_dir) {
  ProfileAttributesEntry* entry =
      storage->GetProfileAttributesWithPath(profile_dir);
  return entry && entry->IsEphemeral();
}

// Disables sync in order to prevent that browsing data deletions propagate
// across devices via sync.
void DisableSyncForProfileDeletion(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // Nothing to do as the user is signed out (hence sync is guaranteed to be
    // disabled).
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, profile deletion uses a different codepath but some
  // browser tests do exercise this code.
  CHECK_IS_TEST();
#else
  identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kSignoutDuringProfileDeletion);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

DeleteProfileHelper::DeleteProfileHelper(ProfileManager& profile_manager)
    : profile_manager_(profile_manager) {}

DeleteProfileHelper::~DeleteProfileHelper() = default;

void DeleteProfileHelper::MaybeScheduleProfileForDeletion(
    const base::FilePath& profile_dir,
    ProfileLoadedCallback callback,
    ProfileMetrics::ProfileDelete deletion_source) {
  if (!ScheduleProfileDirectoryForDeletion(profile_dir))
    return;

  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile_dir);
  if (entry) {
    storage.RecordDeletedProfileState(entry);
  }
  ProfileMetrics::LogProfileDeleteUser(deletion_source);

  DCHECK(profiles::IsMultipleProfilesEnabled());
  DCHECK(!IsProfileDirectoryMarkedForDeletion(profile_dir));

  // When this is called all browser windows may be about to be destroyed
  // (but still exist in BrowserList), which means shutdown may be about to
  // start. Use a KeepAlive to ensure shutdown doesn't start.
  std::unique_ptr<ScopedKeepAlive> keep_alive =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::PROFILE_MANAGER,
                                        KeepAliveRestartOption::DISABLED);

  Profile* profile = profile_manager_->GetProfileByPath(profile_dir);
  if (profile) {
    // Cancel all in-progress downloads before deleting the profile to prevent a
    // "Do you want to exit Google Chrome and cancel the downloads?" prompt
    // (crbug.com/336725).
    DownloadCoreService* service =
        DownloadCoreServiceFactory::GetForBrowserContext(profile);
    service->CancelDownloads(
        DownloadCoreService::CancelDownloadsTrigger::kProfileDeletion);
    DCHECK_EQ(0, service->BlockingShutdownCount());

    // Take a ScopedProfileKeepAlive for the the deletion process to avoid the
    // profile from being randomly unloaded.
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive =
        std::make_unique<ScopedProfileKeepAlive>(
            profile, ProfileKeepAliveOrigin::kProfileDeletionProcess);

    // Close all browser windows before deleting the profile. If the user
    // cancels the closing of any tab in an OnBeforeUnload event, profile
    // deletion is also cancelled. (crbug.com/289390)
    BrowserList::CloseAllBrowsersWithProfile(
        profile,
        base::BindRepeating(
            &DeleteProfileHelper::EnsureActiveProfileExistsBeforeDeletion,
            base::Unretained(this), base::Passed(std::move(keep_alive)),
            base::Passed(std::move(profile_keep_alive)),
            base::Passed(std::move(callback))),
        base::BindRepeating(&CancelProfileDeletion), false);
  } else {
    EnsureActiveProfileExistsBeforeDeletion(std::move(keep_alive),
                                            /*profile_keep_alive=*/nullptr,
                                            std::move(callback), profile_dir);
  }
}

void DeleteProfileHelper::ScheduleEphemeralProfileForDeletion(
    const base::FilePath& profile_dir,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive) {
  DCHECK(IsRegisteredAsEphemeral(
      &profile_manager_->GetProfileAttributesStorage(), profile_dir));
  DCHECK_EQ(0u, chrome::GetBrowserCount(
                    profile_manager_->GetProfileByPath(profile_dir)));
  std::optional<base::FilePath> new_active_profile_dir =
      profile_manager_->FindLastActiveProfile(base::BindRepeating(
          [](const base::FilePath& profile_dir, ProfileAttributesEntry* entry) {
            return entry->GetPath() != profile_dir;
          },
          profile_dir));
  if (!new_active_profile_dir.has_value())
    new_active_profile_dir =
        profile_manager_->GenerateNextProfileDirectoryPath();
  DCHECK(!new_active_profile_dir->empty());
  RemoveFromLastActiveProfilesPrefList(profile_dir);

  FinishDeletingProfile(profile_dir, new_active_profile_dir.value(),
                        std::move(keep_alive));
}

void DeleteProfileHelper::CleanUpEphemeralProfiles() {
  base::FilePath last_used_profile_base_name =
      profile_manager_->GetLastUsedProfileBaseName();
  bool last_active_profile_deleted = false;
  base::FilePath new_profile_path;
  std::vector<base::FilePath> profiles_to_delete;
  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    if (entry->IsEphemeral()) {
      profiles_to_delete.push_back(profile_path);
      RemoveFromLastActiveProfilesPrefList(profile_path);
      if (profile_path.BaseName() == last_used_profile_base_name)
        last_active_profile_deleted = true;
    } else if (new_profile_path.empty()) {
      new_profile_path = profile_path;
    }
  }

  // If the last active profile was ephemeral or all profiles are deleted due to
  // ephemeral, set a new one.
  if (last_active_profile_deleted ||
      (entries.size() == profiles_to_delete.size() &&
       !profiles_to_delete.empty())) {
    if (new_profile_path.empty())
      new_profile_path = profile_manager_->GenerateNextProfileDirectoryPath();

    profiles::SetLastUsedProfile(new_profile_path.BaseName());
  }

  for (const base::FilePath& profile_path : profiles_to_delete) {
    DCHECK(!profile_manager_->GetProfileByPath(profile_path));
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDisk, profile_path,
                       base::OnceClosure()));

    storage.RemoveProfile(profile_path);
  }
}

void DeleteProfileHelper::CleanUpDeletedProfiles() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::Value::List& deleted_profiles =
      local_state->GetList(prefs::kProfilesDeleted);

  for (const base::Value& value : deleted_profiles) {
    std::optional<base::FilePath> profile_path = base::ValueToFilePath(value);
    // Although it should never happen, make sure this is a valid path in the
    // user_data_dir, so we don't accidentally delete something else.
    if (profile_path && profile_manager_->IsAllowedProfilePath(*profile_path)) {
      if (base::PathExists(*profile_path)) {
        LOG(WARNING) << "Files of a deleted profile still exist after restart. "
                        "Cleaning up now.";
        DCHECK(!profile_manager_->GetProfileByPath(*profile_path));
        base::ThreadPool::PostTask(
            FROM_HERE,
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
            base::BindOnce(&NukeProfileFromDisk, *profile_path,
                           base::BindOnce(&ProfileCleanedUp, value.Clone())));
      } else {
        // Everything is fine, the profile was removed on shutdown.
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&ProfileCleanedUp, value.Clone()));
      }
    } else {
      LOG(ERROR) << "Found invalid profile path in deleted_profiles: "
                 << profile_path->AsUTF8Unsafe();
      SCOPED_CRASH_KEY_STRING256("DeleteProfileHelper", "profile_path",
                                 profile_path->AsUTF8Unsafe());
      SCOPED_CRASH_KEY_STRING256(
          "DeleteProfileHelper", "user_data_dir",
          profile_manager_->user_data_dir().AsUTF8Unsafe());
      SCOPED_CRASH_KEY_BOOL(
          "DeleteProfileHelper", "allowed_path",
          profile_manager_->IsAllowedProfilePath(*profile_path));
      base::debug::DumpWithoutCrashing();
    }
  }
}

void DeleteProfileHelper::EnsureActiveProfileExistsBeforeDeletion(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    ProfileLoadedCallback callback,
    const base::FilePath& profile_dir) {
  DCHECK(keep_alive);
  // In case we delete non-active profile and current profile is valid, proceed.
  const base::FilePath last_used_profile_path =
      profile_manager_->GetLastUsedProfileDir();
  const base::FilePath guest_profile_path =
      profile_manager_->GetGuestProfilePath();
  Profile* last_used_profile =
      profile_manager_->GetProfileByPath(last_used_profile_path);
  if (last_used_profile_path != profile_dir &&
      last_used_profile_path != guest_profile_path && last_used_profile) {
    FinishDeletingProfile(profile_dir, last_used_profile_path,
                          std::move(profile_keep_alive));
    return;
  }

  // Search for an active browser and use its profile as active if possible.
  for (Browser* browser : *BrowserList::GetInstance()) {
    Profile* profile = browser->profile();
    base::FilePath cur_path = profile->GetPath();
    if (cur_path != profile_dir && cur_path != guest_profile_path &&
        !IsProfileDirectoryMarkedForDeletion(cur_path)) {
      OnNewActiveProfileInitialized(profile_dir, cur_path, std::move(callback),
                                    std::move(keep_alive),
                                    std::move(profile_keep_alive), profile);
      return;
    }
  }

  // There no valid browsers to fallback, search for any existing valid profile.
  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  base::FilePath fallback_profile_path;
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath cur_path = entry->GetPath();
    // Make sure that this profile is not pending deletion.
    if (cur_path != profile_dir && cur_path != guest_profile_path &&
        !IsProfileDirectoryMarkedForDeletion(cur_path)) {
      fallback_profile_path = cur_path;
      break;
    }
  }

  // If we're deleting the last profile, then create a new profile in its place.
  // Load existing profile otherwise.
  if (fallback_profile_path.empty()) {
    fallback_profile_path =
        profile_manager_->GenerateNextProfileDirectoryPath();
    // A new profile about to be created.
    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_USER_LAST_DELETED);
  }

  // Create and/or load fallback profile.
  profile_manager_->CreateProfileAsync(
      fallback_profile_path,
      base::BindOnce(&DeleteProfileHelper::OnNewActiveProfileInitialized,
                     base::Unretained(this), profile_dir, fallback_profile_path,
                     std::move(callback), std::move(keep_alive),
                     std::move(profile_keep_alive)));
}

void DeleteProfileHelper::FinishDeletingProfile(
    const base::FilePath& profile_dir,
    const base::FilePath& new_active_profile_dir,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive) {
  // Update the last used profile pref before closing browser windows. This
  // way the correct last used profile is set for any notification observers.
  profiles::SetLastUsedProfile(new_active_profile_dir.BaseName());

  // Attempt to load the profile before deleting it to properly clean up
  // profile-specific data stored outside the profile directory.
  profile_manager_->LoadProfileByPath(
      profile_dir, false,
      base::BindOnce(&DeleteProfileHelper::OnLoadProfileForProfileDeletion,
                     base::Unretained(this), profile_dir,
                     std::move(keep_alive)));
  if (!IsProfileDirectoryMarkedForDeletion(profile_dir)) {
    // Prevents CreateProfileAsync from re-creating the profile.
    MarkProfileDirectoryForDeletion(profile_dir);
  }
}

void DeleteProfileHelper::OnLoadProfileForProfileDeletion(
    const base::FilePath& profile_dir,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive,
    Profile* profile) {
  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();

  if (!IsProfileDirectoryMarkedForDeletion(profile_dir)) {
    // Ensure RemoveProfile() knows to nuke the profile directory after it's
    // done.
    MarkProfileDirectoryForDeletion(profile_dir);
  }

  if (profile) {
    // TODO(estade): Migrate additional code in this block to observe
    // ProfileManager instead of handling shutdown here.
    profile_manager_->NotifyOnProfileMarkedForPermanentDeletion(profile);

    // Sign out from doomed profile to avoid that RemoveBrowsingDataForProfile()
    // would result in deletions being propagated to the server (and other
    // devices) via sync.
    DisableSyncForProfileDeletion(profile);

    // The Profile Data doesn't get wiped until Chrome closes. Since we promised
    // that the user's data would be removed, do so immediately.
    //
    // With DestroyProfileOnBrowserClose, this adds a KeepAlive. So the Profile*
    // only gets deleted *after* browsing data is removed. This also clears some
    // keepalives in the process, e.g. due to background extensions getting
    // uninstalled.
    profiles::RemoveBrowsingDataForProfile(profile_dir);

    // Clean-up pref data that won't be cleaned up by deleting the profile dir.
    profile->GetPrefs()->OnStoreDeletionFromDisk();
  } else {
    // We failed to load the profile, but it's safe to delete a not yet loaded
    // Profile from disk.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDisk, profile_dir, base::OnceClosure()));
  }

  storage.RemoveProfile(profile_dir);

  if (profile &&
      base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    // Allow the Profile* to be deleted, even if it had no browser windows.
    profile_manager_->ClearFirstBrowserWindowKeepAlive(profile);
  }
}

void DeleteProfileHelper::OnNewActiveProfileInitialized(
    const base::FilePath& profile_to_delete_path,
    const base::FilePath& new_active_profile_path,
    ProfileLoadedCallback callback,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    Profile* loaded_profile) {
  DCHECK(keep_alive);
  DCHECK(loaded_profile);
  if (IsProfileDirectoryMarkedForDeletion(new_active_profile_path)) {
    // If the profile we tried to load as the next active profile has been
    // deleted, then retry deleting this profile to redo the logic to load
    // the next available profile.
    EnsureActiveProfileExistsBeforeDeletion(
        std::move(keep_alive), std::move(profile_keep_alive),
        std::move(callback), profile_to_delete_path);
    return;
  }

  FinishDeletingProfile(profile_to_delete_path, new_active_profile_path,
                        std::move(profile_keep_alive));
  std::move(callback).Run(loaded_profile);
}
