// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"

namespace base {
class SequencedTaskRunner;
}

namespace gfx {
class Image;
}

class AccountId;
class PrefService;
class ProfileAttributesEntry;
class ProfileAvatarDownloader;

class ProfileAttributesStorage
    : public base::SupportsWeakPtr<ProfileAttributesStorage> {
 public:
  using Observer = ProfileInfoCacheObserver;

  explicit ProfileAttributesStorage(PrefService* prefs);
  virtual ~ProfileAttributesStorage();

  // If the |supervised_user_id| is non-empty, the profile will be marked to be
  // omitted from the avatar-menu list on desktop versions. This is used while a
  // supervised user is in the process of being registered with the server. Use
  // ProfileAttributesEntry::SetIsOmitted() to clear the flag when the profile
  // is ready to be shown in the menu.
  virtual void AddProfile(const base::FilePath& profile_path,
                          const base::string16& name,
                          const std::string& gaia_id,
                          const base::string16& user_name,
                          bool is_consented_primary_account,
                          size_t icon_index,
                          const std::string& supervised_user_id,
                          const AccountId& account_id) = 0;

  // Removes the profile matching given |account_id| from this storage.
  // Calculates profile path and calls RemoveProfile() on it.
  virtual void RemoveProfileByAccountId(const AccountId& account_id) = 0;

  // Removes the profile at |profile_path| from this storage. Does not delete or
  // affect the actual profile's data.
  virtual void RemoveProfile(const base::FilePath& profile_path) = 0;

  // Returns a vector containing one attributes entry per known profile. They
  // are not sorted in any particular order.
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributes();
  std::vector<ProfileAttributesEntry*> GetAllProfilesAttributesSortedByName();

  // Populates |entry| with the data for the profile at |path| and returns true
  // if the operation is successful and |entry| can be used. Returns false
  // otherwise.
  // |entry| should not be cached as it may not reflect subsequent changes to
  // the profile's metadata.
  virtual bool GetProfileAttributesWithPath(
      const base::FilePath& path, ProfileAttributesEntry** entry) = 0;

  // Returns the count of known profiles.
  virtual size_t GetNumberOfProfiles() const = 0;

  // Returns a unique name that can be assigned to a newly created profile.
  base::string16 ChooseNameForNewProfile(size_t icon_index) const;

  // Determines whether |name| is one of the default assigned names.
  // On Desktop, if |include_check_for_legacy_profile_name| is false,
  // |IsDefaultProfileName()| would only return true if the |name| is in the
  // form of |Person %n| which is the new default local profile name. If
  // |include_check_for_legacy_profile_name| is true, we will also check if name
  // is one of the legacy profile names (e.g. Saratoga, Default user, ..).
  // For other platforms, so far |include_check_for_legacy_profile_name|
  // is not used.
  bool IsDefaultProfileName(const base::string16& name,
                            bool include_check_for_legacy_profile_name) const;

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

 protected:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest, EntriesInAttributesStorage);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           DownloadHighResAvatarTest);
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           NothingToDownloadHighResAvatarTest);

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
                             const base::FilePath& image_path);

  PrefService* const prefs_;
  mutable std::unordered_map<base::FilePath::StringType,
                             std::unique_ptr<ProfileAttributesEntry>>
      profile_attributes_entries_;

  mutable base::ObserverList<Observer>::Unchecked observer_list_;

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
  // or when the ProfileInfoCache is destroyed.
  std::unordered_map<std::string, std::unique_ptr<ProfileAvatarDownloader>>
      avatar_images_downloads_in_progress_;

  // Determines of the ProfileAvatarDownloader should be created and executed
  // or not. Only set to true for tests.
  bool disable_avatar_download_for_testing_ = false;

  // Task runner used for file operation on avatar images.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

 private:
  // Called when the picture given by |key| has been loaded from disk and
  // decoded into |image|.
  void OnAvatarPictureLoaded(const base::FilePath& profile_path,
                             const std::string& key,
                             gfx::Image image) const;

  // Called when the picture given by |file_name| has been saved to disk. Used
  // both for the GAIA profile picture and the high res avatar files.
  void OnAvatarPictureSaved(const std::string& file_name,
                            const base::FilePath& profile_path) const;

  // Notifies observers.
  void NotifyOnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) const;

  DISALLOW_COPY_AND_ASSIGN(ProfileAttributesStorage);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_STORAGE_H_
