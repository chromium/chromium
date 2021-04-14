// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_info_interface.h"
#include "components/signin/public/base/persistent_repeating_timer.h"

namespace gfx {
class Image;
}

namespace base {
class DictionaryValue;
}

class PrefService;
class PrefRegistrySimple;

// This class saves various information about profiles to local preferences.
// This cache can be used to display a list of profiles without having to
// actually load the profiles from disk.
// The ProfileInfoInterface is being deprecated. Prefer using the
// ProfileAttributesStorage and avoid using the Get*AtIndex family of functions.
class ProfileInfoCache : public ProfileInfoInterface,
                         public ProfileAttributesStorage,
                         public base::SupportsWeakPtr<ProfileInfoCache> {
 public:
  ProfileInfoCache(PrefService* prefs, const base::FilePath& user_data_dir);
  ProfileInfoCache(const ProfileInfoCache&) = delete;
  ProfileInfoCache& operator=(const ProfileInfoCache&) = delete;
  ~ProfileInfoCache() override;

  // If the |supervised_user_id| is non-empty, the profile will be marked to be
  // omitted from the avatar-menu list on desktop versions. This is used while a
  // supervised user is in the process of being registered with the server. Use
  // SetIsOmittedProfileAtIndex() to clear the flag when the profile is ready to
  // be shown in the menu.
  // Deprecated. Use AddProfile instead.
  void AddProfileToCache(const base::FilePath& profile_path,
                         const std::u16string& name,
                         const std::string& gaia_id,
                         const std::u16string& user_name,
                         bool is_consented_primary_account,
                         size_t icon_index,
                         const std::string& supervised_user_id,
                         const AccountId& account_id);
  // Deprecated. Use RemoveProfile instead.
  void DeleteProfileFromCache(const base::FilePath& profile_path);

  // ProfileInfoInterface:
  size_t GetNumberOfProfiles(bool include_guest_profile = false) const override;
  // Don't cache this value and reuse, because resorting the menu could cause
  // the item being referred to to change out from under you.
  // Deprecated. Prefer using the ProfileAttributesStorage interface instead of
  // directly referring to this implementation.
  size_t GetIndexOfProfileWithPath(
      const base::FilePath& profile_path) const override;
  // Will be removed SOON with ProfileInfoCache tests. Do not use!
  base::FilePath GetPathOfProfileAtIndex(size_t index) const override;
  // Returns the GAIA picture for the given profile. This may return NULL
  // if the profile does not have a GAIA picture or if the picture must be
  // loaded from disk.
  const gfx::Image* GetGAIAPictureOfProfileAtIndex(size_t index) const override;
  bool IsUsingGAIAPictureOfProfileAtIndex(size_t index) const override;
  bool ProfileIsUsingDefaultAvatarAtIndex(size_t index) const override;

  // Returns true if a GAIA picture has been loaded or has failed to load for
  // profile at |index|.
  bool IsGAIAPictureOfProfileAtIndexLoaded(size_t index) const;
  // Will be removed SOON with ProfileInfoCache tests. Do not use!
  size_t GetAvatarIconIndexOfProfileAtIndex(size_t index) const;

  // Will be removed SOON with ProfileInfoCache tests. Do not use!
  void SetAvatarIconOfProfileAtIndex(size_t index, size_t icon_index);
  void SetGAIAPictureOfProfileAtIndex(size_t index,
                                      const std::string& image_url_with_size,
                                      gfx::Image image);

  void SetIsUsingGAIAPictureOfProfileAtIndex(size_t index, bool value);
  void SetProfileIsUsingDefaultAvatarAtIndex(size_t index, bool value);

  // Notify IsSignedInRequired to all observer
  void NotifyIsSigninRequiredChanged(const base::FilePath& profile_path);

  const base::FilePath& GetUserDataDir() const;

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // ProfileAttributesStorage:
  void AddProfile(const base::FilePath& profile_path,
                  const std::u16string& name,
                  const std::string& gaia_id,
                  const std::u16string& user_name,
                  bool is_consented_primary_account,
                  size_t icon_index,
                  const std::string& supervised_user_id,
                  const AccountId& account_id) override;
  void RemoveProfileByAccountId(const AccountId& account_id) override;
  void RemoveProfile(const base::FilePath& profile_path) override;

  ProfileAttributesEntry* GetProfileAttributesWithPath(
      const base::FilePath& path) override;
  void DisableProfileMetricsForTesting() override;

  void NotifyProfileAuthInfoChanged(const base::FilePath& profile_path);
  void NotifyIfProfileNamesHaveChanged();
  void NotifyProfileSupervisedUserIdChanged(const base::FilePath& profile_path);
  void NotifyProfileIsOmittedChanged(const base::FilePath& profile_path);
  void NotifyProfileThemeColorsChanged(const base::FilePath& profile_path);
  void NotifyProfileHostedDomainChanged(const base::FilePath& profile_path);

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileAttributesStorageTest,
                           DownloadHighResAvatarTest);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest,
                           MigrateLegacyProfileNamesAndRecomputeIfNeeded);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest, PersistGAIAPicture);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest, EmptyGAIAInfo);

  const base::DictionaryValue* GetInfoForProfileAtIndex(size_t index) const;
  // Saves the profile info to a cache.
  void SetInfoForProfileAtIndex(size_t index,
                                std::unique_ptr<base::DictionaryValue> info);
  std::string CacheKeyFromProfilePath(const base::FilePath& profile_path) const;
  std::vector<std::string>::iterator FindPositionForProfile(
      const std::string& search_key,
      const std::u16string& search_name);

  // Updates the position of the profile at the given index so that the list
  // of profiles is still sorted.
  void UpdateSortForProfileIndex(size_t index);

  // Will be removed SOON with ProfileInfoCache tests. Do not use!
  // Loads or uses an already loaded high resolution image of the
  // generic profile avatar.
  const gfx::Image* GetHighResAvatarOfProfileAtIndex(size_t index) const;

  // Download and high-res avatars used by the profiles.
  void DownloadAvatars();

  std::string GetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(
      size_t index) const;

  void SetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(
      size_t index,
      const std::string& image_url_with_size);

  bool ShouldUpdateGAIAPictureOfProfileAtIndex(
      size_t index,
      const std::string& old_file_name,
      const std::string& key,
      const std::string& image_url_with_size,
      bool image_is_empty) const;
#if !defined(OS_ANDROID)
  void LoadGAIAPictureIfNeeded();
#endif

  ProfileAttributesEntry* InitEntryWithKey(const std::string& key);

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Migrate any legacy profile names ("First user", "Default Profile") to
  // new style default names ("Person 1"). Rename any duplicates of "Person n"
  // i.e. Two or more profiles with the profile name "Person 1" would be
  // recomputed to "Person 1" and "Person 2".
  void MigrateLegacyProfileNamesAndRecomputeIfNeeded();
  static void SetLegacyProfileMigrationForTesting(bool value);

  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

  std::vector<std::string> keys_;
  const base::FilePath user_data_dir_;
  base::WeakPtrFactory<ProfileInfoCache> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
