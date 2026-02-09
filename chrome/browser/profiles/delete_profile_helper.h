// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_DELETE_PROFILE_HELPER_H_
#define CHROME_BROWSER_PROFILES_DELETE_PROFILE_HELPER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/profiles/profile_metrics.h"

namespace base {
class FilePath;
}

class Profile;
class ProfileManager;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

// This class offers helper functions for profile deletion. Note that
// `DeleteProfileHelper` does not delete actual C++ Profile objects; that is
// managed by `ScopedProfileKeepAlive` and `ProfileManager::RemoveProfile()`.
//
// The `DeleteProfileHelper` is responsible for:
// - Deleting the profile as a user-visible concept: removing it from
//   `ProfileAttributesStorage` and wiping the user data on disk.
// - Ensuring another profile is created or loaded before the last one is
//   deleted.
//
// Lifecycle of a profile deletion:
// 1. Scheduling: `MaybeScheduleProfileForDeletion()` adds the path to
//    `ProfilesToDelete()` map (stage: SCHEDULING).
// 2. Marking: Once browsers are closed, `MarkProfileDirectoryForDeletion()`
//    updates the stage to MARKED, appends the basename to the
//    `kProfilesDeleted` Local State pref, and marks the profile as ephemeral
//    in `ProfileAttributesStorage`.
// 3. Cleanup: The profile is loaded (if not already) to perform polite
//    in-memory cleanup (Sync sign-out, etc.).
// 4. Orphaning: `storage.RemoveProfile()` removes the profile from the known
//    attributes. The directory is now "orphaned" on disk but still tracked in
//    the `kProfilesDeleted` preference.
// 5. Disk Deletion:
//    - On load failure: Nuked immediately.
//    - On load success: Nuked on the next startup to avoid file locks while the
//      Profile object is still alive in memory.
//
// Startup Fail-safes:
// To survive crashes, `PreProfileInit()` calls two methods:
// - `CleanUpEphemeralProfiles()`: Scans storage for profiles marked ephemeral.
//   This handles profiles where deletion was interrupted before they could be
//   removed from storage (step 4).
// - `CleanUpDeletedProfiles()`: Reconstructs absolute paths from
//   `kProfilesDeleted` and nukes them. This is the primary way directories are
//   cleaned up after a successful "polite" session, or if storage was
//   corrupted.
class DeleteProfileHelper {
 public:
  using ProfileLoadedCallback = base::OnceCallback<void(Profile*)>;

  explicit DeleteProfileHelper(ProfileManager& profile_manager);

  ~DeleteProfileHelper();

  DeleteProfileHelper(const DeleteProfileHelper&) = delete;
  DeleteProfileHelper& operator=(const DeleteProfileHelper&) = delete;

  // Schedules the profile at `profile_dir` to be deleted. If deleting the last
  // profile, a new one is created and `callback` is called upon completion.
  // Silently exits if deletion is already in progress. May trigger a profile
  // load to ensure polite cleanup of profile-specific data (e.g., Sync).
  void MaybeScheduleProfileForDeletion(
      const base::FilePath& profile_dir,
      ProfileLoadedCallback callback,
      ProfileMetrics::ProfileDelete deletion_source);

  // Schedules an ephemeral profile at `profile_dir` for deletion. No new
  // profiles are created.
  void ScheduleEphemeralProfileForDeletion(
      const base::FilePath& profile_dir,
      std::unique_ptr<ScopedProfileKeepAlive> keep_alive);

  // Fail-safe cleanup methods called on startup to remove directories left
  // behind by a crash. These only perform disk operations and do not load the
  // profiles.
  // `CleanUpEphemeralProfiles()` nukes profiles marked as ephemeral in the
  // `ProfileAttributesStorage`.
  // `CleanUpDeletedProfiles()` nukes directories listed in the
  // `kProfilesDeleted` preference.
  void CleanUpEphemeralProfiles();
  void CleanUpDeletedProfiles();

 private:
  // Continues the scheduled profile deletion after closing all the profile's
  // browsers tabs. Creates a new profile if the profile to be deleted is the
  // last non-supervised profile.
  // `profile_keep_alive` is used to avoid unloading the profile during the
  // deletion process and is null if the profile is not loaded.
  void EnsureActiveProfileExistsBeforeDeletion(
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ProfileLoadedCallback callback,
      const base::FilePath& profile_dir);

  // Schedules the profile at the given path to be deleted on shutdown,
  // and marks the new profile as active.
  void FinishDeletingProfile(
      const base::FilePath& profile_dir,
      const base::FilePath& new_active_profile_dir,
      std::unique_ptr<ScopedProfileKeepAlive> keep_alive);
  void OnLoadProfileForProfileDeletion(
      const base::FilePath& profile_dir,
      std::unique_ptr<ScopedProfileKeepAlive> keep_alive,
      Profile* profile);

  // If the `loaded_profile` has been loaded successfully and isn't already
  // scheduled for deletion, then finishes adding `profile_to_delete_path` to
  // the queue of profiles to be deleted, and updates the kProfileLastUsed
  // preference based on `new_active_profile_path`. `keep_alive` may be null and
  // is used to ensure shutdown does not start. `profile_keep_alive` is used to
  // avoid unloading the profile during the deletion process and is null if the
  // profile is not loaded.
  void OnNewActiveProfileInitialized(
      const base::FilePath& profile_to_delete_path,
      const base::FilePath& new_active_profile_path,
      ProfileLoadedCallback callback,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      Profile* loaded_profile);

  const raw_ref<ProfileManager>
      profile_manager_;  // Owns the `DeleteProfileHelper`.
};

#endif  // CHROME_BROWSER_PROFILES_DELETE_PROFILE_HELPER_H_
