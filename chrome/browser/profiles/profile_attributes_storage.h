// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"

namespace base {
class SequencedTaskRunner;
}

namespace gfx {
class Image;
}

namespace signin {
class PersistentRepeatingTimer;
}

class AccountId;
class PrefService;
class ProfileAttributesEntry;
class ProfileAvatarDownloader;
class PrefRegistrySimple;

class ProfileAttributesStorage {
 public:
  using Observer = ProfileAttributesStorageObserver;

  explicit ProfileAttributesStorage(PrefService* prefs,
                                    const base::FilePath& user_data_dir);
  ProfileAttributesStorage(const ProfileAttributesStorage&) = delete;
  ProfileAttributesStorage& operator=(const ProfileAttributesStorage&) = delete;
  ~ProfileAttributesStorage();

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Return the keys for all the profiles; exposed as a static method so that
  // it can be called very early in Chrome initialization.
  static base::flat_set<std::string> GetAllProfilesKeys(
      PrefService* local_prefs);

  // Adds a new profile with `params` to the attributes storage.
  // `params.profile_path` must be a valid path within the user data directory
  // that hasn't been registered with this `ProfileAttributesStorage` before.
  void AddProfile(ProfileAttributesInitParams params);

  // Removes the profile matching given |account_id| from this storage.
  // Calculates profile path and calls RemoveProfile() on it.
  void RemoveProfileByAccountId(const AccountId& account_id);

  // Removes the profile at |profile_path| from this storage. Does not delete or
  // affect the actual profile's data.
  void RemoveProfile(const base::FilePath& profile_path);

  // Returns a vector containing one attributes entry per known profile.
  // They are not sorted in any particular order.
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributes() const;

  // Return all user profile attributes sorted using the `prefs::kProfilesOrder`
  // profile order stored.
  std::vector<ProfileAttributesEntry*>
  GetAllProfilesAttributesSortedForDisplay() const;

  // Conditionally returns the sorted list based on the feature flag
  // `kProfilesReordering`. It will return the sorted list based on the stored
  // order if the feature is enabled, or the sorted list based on the local
  // profile name if the feature is disabled.
  std::vector<ProfileAttributesEntry*>
  GetAllProfilesAttributesSortedByLocalProfileNameWithCheck() const;

  // Conditionally returns the sorted list based on the feature flag
  // `kProfilesReordering`. It will return the sorted list based on the stored
  // order if the feature is enabled, or the sorted list based on the name if
  // the feature is disabled.
  std::vector<ProfileAttributesEntry*>
  GetAllProfilesAttributesSortedByNameWithCheck() const;

  // Updates `prefs::kProfilesOrder`. Move profile keys at `from_index` and
  // place it at `to_index` shifting all keys in between by 1 spot.
  void UpdateProfilesOrderPref(size_t from_index, size_t to_index);

  // Returns a ProfileAttributesEntry with the data for the profile at |path|
  // if the operation is successful. Returns |nullptr| otherwise.
  // Returned value should not be cached because the profile entry may be
  // deleted at any time, an then using this value would cause use-after-free.
  ProfileAttributesEntry* GetProfileAttributesWithPath(
      const base::FilePath& path);

  // Returns the count of known profiles.
  size_t GetNumberOfProfiles() const;

  // Returns a unique name that can be assigned to a newly created profile.
  std::u16string ChooseNameForNewProfile(size_t icon_index) const;

  // Determines whether |name| is one of the default assigned names.
  // On Desktop, if |include_check_for_legacy_profile_name| is false,
  // |IsDefaultProfileName()| would only return true if the |name| is in the
  // form of |Person %n| which is the new default local profile name. If
  // |include_check_for_legacy_profile_name| is true, we will also check if name
  // is one of the legacy profile names (e.g. Saratoga, Default user, ..).
  // For other platforms, so far |include_check_for_legacy_profile_name|
  // is not used.
  bool IsDefaultProfileName(const std::u16string& name,
                            bool include_check_for_legacy_profile_name) const;

#if !BUILDFLAG(IS_ANDROID)
  // Records statistics about a profile `entry` that is being deleted. If the
  // profile has opened browser window(s) in the moment of deletion, this
  // function must be called before these windows get closed.
  void RecordDeletedProfileState(ProfileAttributesEntry* entry);
#endif

  // Records statistics about profiles as would be visible in the profile picker
  // (if we would display it in this moment).
  void RecordProfilesState();

  // Returns an avatar icon index that can be assigned to a newly created
  // profile. Note that the icon may not be unique since there are a limited
  // set of default icons.
  size_t ChooseAvatarIconIndexForNewProfile() const;

  // Returns the decoded image at |image_path|. Used both by the GAIA profile
  // image and the high res avatars.
  const gfx::Image* LoadAvatarPictureFromPath(
      const base::FilePath& profile_path,
      const std::string& key,
      const base::FilePath& image_path) const;

  // Returns true if a GAIA picture has been loaded or has failed to load for
  // profile with `key`.
  bool IsGAIAPictureLoaded(const std::string& key) const;

  // Saves the GAIA `image` at `image_path`.
  void SaveGAIAImageAtPath(const base::FilePath& profile_path,
                           const std::string& key,
                           gfx::Image image,
                           const base::FilePath& image_path,
                           const std::string& image_url_with_size);
  // Deletes a GAIA picture at `image_path`.
  void DeleteGAIAImageAtPath(const base::FilePath& profile_path,
                             const std::string& key,
                             const base::FilePath& image_path);

  // Checks whether the high res avatar at index |icon_index| exists, and if it
  // does not, calls |DownloadHighResAvatar|.
  void DownloadHighResAvatarIfNeeded(size_t icon_index,
                                     const base::FilePath& profile_path);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool GetDisableAvatarDownloadForTesting() {
    return disable_avatar_download_for_testing_;
  }

  void set_disable_avatar_download_for_testing(
      bool disable_avatar_download_for_testing) {
    disable_avatar_download_for_testing_ = disable_avatar_download_for_testing;
  }

  // Notifies observers. The following methods are accessed by
  // ProfileAttributesEntry.
  void NotifyOnProfileAvatarChanged(const base::FilePath& profile_path) const;
  void NotifyIsSigninRequiredChanged(const base::FilePath& profile_path) const;
  void NotifyProfileAuthInfoChanged(const base::FilePath& profile_path) const;
  void NotifyIfProfileNamesHaveChanged() const;
  void NotifyProfileSupervisedUserIdChanged(
      const base::FilePath& profile_path) const;
  void NotifyProfileIsOmittedChanged(const base::FilePath& profile_path) const;
  void NotifyProfileThemeColorsChanged(
      const base::FilePath& profile_path) const;
  void NotifyProfileHostedDomainChanged(
      const base::FilePath& profile_path) const;
  void NotifyProfileUserManagementAcceptanceChanged(
      const base::FilePath& profile_path) const;
  void NotifyProfileManagementEnrollmentTokenChanged(
      const base::FilePath& profile_path) const;
  void NotifyProfileManagementIdChanged(
      const base::FilePath& profile_path) const;

  // Returns a pref dictionary key of a profile at `profile_path`.
  std::string StorageKeyFromProfilePath(
      const base::FilePath& profile_path) const;

  // Disables the periodic reporting of profile metrics, as this is causing
  // tests to time out.
  void DisableProfileMetricsForTesting();

  void EnsureProfilesOrderPrefIsInitializedForTesting();

  base::WeakPtr<ProfileAttributesStorage> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           DownloadHighResAvatarTest);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           NothingToDownloadHighResAvatarTest);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           MigrateLegacyProfileNamesAndRecomputeIfNeeded);

  // Starts downloading the high res avatar at index |icon_index| for profile
  // with path |profile_path|.
  void DownloadHighResAvatar(size_t icon_index,
                             const base::FilePath& profile_path);

  // Saves the avatar |image| at |image_path|. This is used both for the GAIA
  // profile pictures and the ProfileAvatarDownloader that is used to download
  // the high res avatars.
  void SaveAvatarImageAtPath(const base::FilePath& profile_path,
                             gfx::Image image,
                             const std::string& key,
                             const base::FilePath& image_path,
                             base::OnceClosure callback);

  // Returns all non-Guest profile attributes sorted by name.
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributesSortedByName()
      const;

  // Returns all non-Guest profile attributes sorted by local profile name.
  std::vector<ProfileAttributesEntry*>
  GetAllProfilesAttributesSortedByLocalProfileName() const;

  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributesSorted(
      bool use_local_profile_name) const;

  // Makes sure that the pref `prefs::kProfilesOrder` is properly initialized
  // with the existing profiles.
  void EnsureProfilesOrderPrefIsInitialized();

  // Returns whether the list in `prefs::kProfilesOrder` is consistent with the
  // profile entries.
  bool IsProfilesOrderPrefValid() const;

  // Returns a constructed map of storage key to each `ProfileAttributesEntry`.
  base::flat_map<std::string, ProfileAttributesEntry*> GetStorageKeyEntryMap()
      const;

  // Creates and initializes a ProfileAttributesEntry with `key`. `is_omitted`
  // indicates whether the profile should be hidden in UI.
  ProfileAttributesEntry* InitEntryWithKey(const std::string& key,
                                           bool is_omitted);

  // Download and high-res avatars used by the profiles.
  void DownloadAvatars();

#if !BUILDFLAG(IS_ANDROID)
  // Loads GAIA pictures (if any) for all profiles registered in the storage and
  // puts them in memory cache.
  void LoadGAIAPictureIfNeeded();
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Migrate any legacy profile names ("First user", "Default Profile") to
  // new style default names ("Person 1"). Rename any duplicates of "Person n"
  // i.e. Two or more profiles with the profile name "Person 1" would be
  // recomputed to "Person 1" and "Person 2".
  void MigrateLegacyProfileNamesAndRecomputeIfNeeded();
  static void SetLegacyProfileMigrationForTesting(bool value);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  // Called when the picture given by |key| has been loaded from disk and
  // decoded into |image|.
  void OnAvatarPictureLoaded(const base::FilePath& profile_path,
                             const std::string& key,
                             gfx::Image image) const;

  // Called when the picture given by |file_name| has been saved to disk. Used
  // both for the GAIA profile picture and the high res avatar files.
  void OnAvatarPictureSaved(const std::string& file_name,
                            const base::FilePath& profile_path,
                            base::OnceClosure callback,
                            bool success) const;

  // Called when the GAIA picture given by `image_url_with_size` has been saved
  // to disk.
  void OnGAIAPictureSaved(const std::string& image_url_with_size,
                          const base::FilePath& profile_path);

  // Helper function that calls SaveAvatarImageAtPath without a callback.
  void SaveAvatarImageAtPathNoCallback(const base::FilePath& profile_path,
                                       gfx::Image image,
                                       const std::string& key,
                                       const base::FilePath& image_path);

  // Notifies observers.
  void NotifyOnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) const;

  const raw_ptr<PrefService> prefs_;
  mutable std::unordered_map<base::FilePath::StringType, ProfileAttributesEntry>
      profile_attributes_entries_;

  mutable base::ObserverList<Observer>::UncheckedAndDanglingUntriaged
      observer_list_;

  // A cache of gaia/high res avatar profile pictures. This cache is updated
  // lazily so it needs to be mutable.
  mutable std::unordered_map<std::string, gfx::Image> cached_avatar_images_;

  // Marks a profile picture as loading from disk. This prevents a picture from
  // loading multiple times.
  mutable std::unordered_map<std::string, bool> cached_avatar_images_loading_;

  // Hash table of profile pictures currently being downloaded from the remote
  // location and the ProfileAvatarDownloader instances downloading them.
  // This prevents a picture from being downloaded multiple times. The
  // ProfileAvatarDownloader instances are deleted when the download completes
  // or when the ProfileAttributesStorage is destroyed.
  std::unordered_map<std::string, std::unique_ptr<ProfileAvatarDownloader>>
      avatar_images_downloads_in_progress_;

  // Determines of the ProfileAvatarDownloader should be created and executed
  // or not. Only set to true for tests.
  bool disable_avatar_download_for_testing_ = false;

  // Task runner used for file operation on avatar images.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  const base::FilePath user_data_dir_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // PersistentRepeatingTimer for periodically logging profile metrics.
  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  base::WeakPtrFactory<ProfileAttributesStorage> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
