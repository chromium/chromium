// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/signin/public/base/persistent_repeating_timer.h"

class PrefService;
class PrefRegistrySimple;

// This class saves various information about profiles to local preferences.
// This cache can be used to display a list of profiles without having to
// actually load the profiles from disk.
class ProfileInfoCache : public ProfileAttributesStorage {
 public:
  ProfileInfoCache(PrefService* prefs, const base::FilePath& user_data_dir);
  ProfileInfoCache(const ProfileInfoCache&) = delete;
  ProfileInfoCache& operator=(const ProfileInfoCache&) = delete;
  ~ProfileInfoCache() override;

  // Deprecated. Use AddProfile instead.
  void AddProfileToCache(ProfileAttributesInitParams params);
  // Deprecated. Use RemoveProfile instead.
  void DeleteProfileFromCache(const base::FilePath& profile_path);

  const base::FilePath& GetUserDataDir() const;

  // Register cache related preferences in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // ProfileAttributesStorage:
  void AddProfile(ProfileAttributesInitParams) override;
  void RemoveProfileByAccountId(const AccountId& account_id) override;
  void RemoveProfile(const base::FilePath& profile_path) override;

  void DisableProfileMetricsForTesting() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoCacheTest,
                           MigrateLegacyProfileNamesAndRecomputeIfNeeded);

  std::string CacheKeyFromProfilePath(const base::FilePath& profile_path) const;

  // Download and high-res avatars used by the profiles.
  void DownloadAvatars();

#if !defined(OS_ANDROID)
  void LoadGAIAPictureIfNeeded();
#endif

  ProfileAttributesEntry* InitEntryWithKey(const std::string& key,
                                           bool is_omitted);

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Migrate any legacy profile names ("First user", "Default Profile") to
  // new style default names ("Person 1"). Rename any duplicates of "Person n"
  // i.e. Two or more profiles with the profile name "Person 1" would be
  // recomputed to "Person 1" and "Person 2".
  void MigrateLegacyProfileNamesAndRecomputeIfNeeded();
  static void SetLegacyProfileMigrationForTesting(bool value);

  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

  const base::FilePath user_data_dir_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_H_
