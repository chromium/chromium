// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_DELETE_PROFILE_HELPER_H_
#define CHROME_BROWSER_PROFILES_DELETE_PROFILE_HELPER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_metrics.h"

namespace base {
class FilePath;
}

class Profile;
class ProfileManager;
class ScopedKeepAlive;

// This class offers a few helper functions for profile deletion. Note that the
// `DeleteProfileHelper` does not delete actual C++ Profile objects, as this is
// done through the `ScopedProfileKeepAlive` mechanism and
// `ProfileManager::RemoveProfile()`.
// The `DeleteProfileHelper` is responsible for:
// - deleting the profile as a user-visible concept: removes it from the
//   `ProfileAttributesStorage` and deletes the user data on disk.
// - creates or loads another profile before the last profile is deleted.
class DeleteProfileHelper {
 public:
  using ProfileLoadedCallback = base::OnceCallback<void(Profile*)>;

  explicit DeleteProfileHelper(ProfileManager& profile_manager);

  ~DeleteProfileHelper();

  DeleteProfileHelper(const DeleteProfileHelper&) = delete;
  DeleteProfileHelper& operator=(const DeleteProfileHelper&) = delete;

  // Schedules the profile at the given path to be deleted on shutdown. If we're
  // deleting the last profile, a new one will be created in its place, and in
  // that case the callback will be called when profile creation is complete.
  // Silently exits if profile is either scheduling or marked for deletion.
  // If the profile is not loaded, this may trigger a reload of the profile so
  // that user data is cleaned up.
  void MaybeScheduleProfileForDeletion(
      const base::FilePath& profile_dir,
      ProfileLoadedCallback callback,
      ProfileMetrics::ProfileDelete deletion_source);

  // Schedules the ephemeral profile at the given path to be deleted. New
  // profiles will not be created. If the profile is not loaded, this may
  // trigger a reload of the profile so that user data is cleaned up.
  void ScheduleEphemeralProfileForDeletion(
      const base::FilePath& profile_dir,
      std::unique_ptr<ScopedProfileKeepAlive> keep_alive);

  // Checks if any profiles are left behind (e.g. because of a browser
  // crash) and schedule them for deletion. Unlike the "Schedule" methods above,
  // these functions only remove the data from disk and do not perform any steps
  // that require the profile to be loaded. They assume that the deleted
  // profiles are not loaded in memory.
  void CleanUpEphemeralProfiles();
  void CleanUpDeletedProfiles();

 private:
  // Continues the scheduled profile deletion after closing all the profile's
  // browsers tabs. Creates a new profile if the profile to be deleted is the
  // last non-supervised profile. In the Mac, loads the next non-supervised
  // profile if the profile to be deleted is the active profile.
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
  // scheduled for deletion, then finishes adding `profile_to_delete_dir` to the
  // queue of profiles to be deleted, and updates the kProfileLastUsed
  // preference based on `last_non_supervised_profile_path`. `keep_alive` may be
  // null and is used to ensure shutdown does not start. `profile_keep_alive` is
  // used to avoid unloading the profile during the deletion process and is null
  // if the profile is not loaded.
  void OnNewActiveProfileInitialized(
      const base::FilePath& profile_to_delete_path,
      const base::FilePath& last_non_supervised_profile_path,
      ProfileLoadedCallback callback,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      Profile* loaded_profile);

  const raw_ref<ProfileManager>
      profile_manager_;  // Owns the `DeleteProfileHelper`.
};

#endif  // CHROME_BROWSER_PROFILES_DELETE_PROFILE_HELPER_H_
