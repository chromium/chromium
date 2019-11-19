// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache_unittest.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using content::BrowserThread;

ProfileNameVerifierObserver::ProfileNameVerifierObserver(
    TestingProfileManager* testing_profile_manager)
    : testing_profile_manager_(testing_profile_manager) {
  DCHECK(testing_profile_manager_);
}

ProfileNameVerifierObserver::~ProfileNameVerifierObserver() {
}

void ProfileNameVerifierObserver::OnProfileAdded(
    const base::FilePath& profile_path) {
  base::string16 profile_name = GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(profile_path) == profile_names_.end());
  profile_names_.insert({profile_path, profile_name});
}

void ProfileNameVerifierObserver::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  auto it = profile_names_.find(profile_path);
  EXPECT_TRUE(it != profile_names_.end());
  profile_names_.erase(it);
}

void ProfileNameVerifierObserver::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  EXPECT_TRUE(profile_names_.find(profile_path) == profile_names_.end());
}

void ProfileNameVerifierObserver::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  base::string16 new_profile_name =
      GetCache()->GetNameToDisplayOfProfileAtIndex(
          GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_[profile_path] == old_profile_name);
  profile_names_[profile_path] = new_profile_name;
}

void ProfileNameVerifierObserver::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  EXPECT_TRUE(profile_names_.find(profile_path) != profile_names_.end());
}

ProfileInfoCache* ProfileNameVerifierObserver::GetCache() {
  return testing_profile_manager_->profile_info_cache();
}

ProfileInfoCacheTest::ProfileInfoCacheTest()
    : testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
      name_observer_(&testing_profile_manager_) {}

ProfileInfoCacheTest::~ProfileInfoCacheTest() {
}

void ProfileInfoCacheTest::SetUp() {
  ASSERT_TRUE(testing_profile_manager_.SetUp());
  testing_profile_manager_.profile_info_cache()->AddObserver(&name_observer_);
}

void ProfileInfoCacheTest::TearDown() {
  // Drain remaining tasks to make sure all tasks are completed. This prevents
  // memory leaks.
  content::RunAllTasksUntilIdle();
}

ProfileInfoCache* ProfileInfoCacheTest::GetCache() {
  return testing_profile_manager_.profile_info_cache();
}

base::FilePath ProfileInfoCacheTest::GetProfilePath(
    const std::string& base_name) {
  return testing_profile_manager_.profile_manager()->user_data_dir().
      AppendASCII(base_name);
}

void ProfileInfoCacheTest::ResetCache() {
  testing_profile_manager_.DeleteProfileInfoCache();
}

class ProfileInfoCacheTestWithParam
    : public ProfileInfoCacheTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ProfileInfoCacheTestWithParam() : ProfileInfoCacheTest() {
    concatenate_enabled_ = GetParam();
    if (concatenate_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(features::kProfileMenuRevamp);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kProfileMenuRevamp);
    }
  }

  base::string16 GetConcatenation(const base::string16& gaia_name,
                                  const base::string16 profile_name) {
    base::string16 name_to_display(gaia_name);
    name_to_display.append(base::UTF8ToUTF16(" ("));
    name_to_display.append(profile_name);
    name_to_display.append(base::UTF8ToUTF16(")"));
    return name_to_display;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool concatenate_enabled_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileInfoCacheTestWithParam);
};

INSTANTIATE_TEST_SUITE_P(ProfileInfoCacheTest,
                         ProfileInfoCacheTestWithParam,
                         testing::Bool());

TEST_P(ProfileInfoCacheTestWithParam, AddProfiles) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
  // Avatar icons not used on Android.
#if !defined(OS_ANDROID)
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
#endif

  for (uint32_t i = 0; i < 4; ++i) {
    base::FilePath profile_path =
        GetProfilePath(base::StringPrintf("path_%ud", i));
    base::string16 profile_name =
        ASCIIToUTF16(base::StringPrintf("name_%ud", i));
#if !defined(OS_ANDROID)
    const SkBitmap* icon = rb.GetImageNamed(
        profiles::GetDefaultAvatarIconResourceIDAtIndex(
            i)).ToSkBitmap();
#endif
    std::string supervised_user_id = "";
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    if (i == 3)
      supervised_user_id = supervised_users::kChildAccountSUID;
#endif

    GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                  base::string16(), false, i,
                                  supervised_user_id, EmptyAccountId());

    ProfileAttributesEntry* entry = nullptr;
    GetCache()->GetProfileAttributesWithPath(profile_path, &entry);
    entry->SetBackgroundStatus(true);
    base::string16 gaia_name = ASCIIToUTF16(base::StringPrintf("gaia_%ud", i));
    GetCache()->SetGAIANameOfProfileAtIndex(i, gaia_name);

    EXPECT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
    base::string16 expected_profile_name =
        concatenate_enabled_ ? GetConcatenation(gaia_name, profile_name)
                             : profile_name;

    EXPECT_EQ(expected_profile_name,
              GetCache()->GetNameToDisplayOfProfileAtIndex(i));

    EXPECT_EQ(profile_path, GetCache()->GetPathOfProfileAtIndex(i));
#if !defined(OS_ANDROID)
    const SkBitmap* actual_icon = entry->GetAvatarIcon().ToSkBitmap();
    EXPECT_EQ(icon->width(), actual_icon->width());
    EXPECT_EQ(icon->height(), actual_icon->height());
#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    EXPECT_EQ(i == 3, GetCache()->ProfileIsSupervisedAtIndex(i));
    EXPECT_EQ(i == 3, GetCache()->IsOmittedProfileAtIndex(i));
#else
    EXPECT_FALSE(GetCache()->ProfileIsSupervisedAtIndex(i));
    EXPECT_FALSE(GetCache()->IsOmittedProfileAtIndex(i));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
    EXPECT_EQ(supervised_user_id,
              GetCache()->GetSupervisedUserIdOfProfileAtIndex(i));
  }

  // Reset the cache and test the it reloads correctly.
  ResetCache();

  EXPECT_EQ(4u, GetCache()->GetNumberOfProfiles());
  for (uint32_t i = 0; i < 4; ++i) {
    base::FilePath profile_path =
          GetProfilePath(base::StringPrintf("path_%ud", i));
    EXPECT_EQ(i, GetCache()->GetIndexOfProfileWithPath(profile_path));
    base::string16 profile_name =
        ASCIIToUTF16(base::StringPrintf("name_%ud", i));
    base::string16 gaia_name = ASCIIToUTF16(base::StringPrintf("gaia_%ud", i));
    base::string16 expected_profile_name =
        concatenate_enabled_ ? GetConcatenation(gaia_name, profile_name)
                             : profile_name;
    EXPECT_EQ(expected_profile_name,
              GetCache()->GetNameToDisplayOfProfileAtIndex(i));
#if !defined(OS_ANDROID)
    EXPECT_EQ(i, GetCache()->GetAvatarIconIndexOfProfileAtIndex(i));
#endif
    ProfileAttributesEntry* entry = nullptr;
    GetCache()->GetProfileAttributesWithPath(profile_path, &entry);
    EXPECT_EQ(true, entry->GetBackgroundStatus());
    EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(i));
  }
}

TEST_P(ProfileInfoCacheTestWithParam, GAIAName) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("Person 1"), std::string(),
      base::string16(), false, 0, std::string(), EmptyAccountId());
  base::string16 profile_name(ASCIIToUTF16("Person 2"));
  GetCache()->AddProfileToCache(GetProfilePath("path_2"), profile_name,
                                std::string(), base::string16(), false, 0,
                                std::string(), EmptyAccountId());

  int index1 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_1"));
  int index2 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_2"));

  // Sanity check.
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(index1).empty());
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(index2).empty());

  // Set GAIA name.
  base::string16 gaia_name(ASCIIToUTF16("Pat Smith"));
  GetCache()->SetGAIANameOfProfileAtIndex(index2, gaia_name);
  // Since there is a GAIA name, we use that as a display name.
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(index1).empty());
  EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(index2));
  EXPECT_EQ(gaia_name, GetCache()->GetNameToDisplayOfProfileAtIndex(index2));

  base::string16 custom_name(ASCIIToUTF16("Custom name"));
  GetCache()->SetLocalProfileNameOfProfileAtIndex(index2, custom_name);
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index2, false);

  base::string16 expected_profile_name =
      concatenate_enabled_ ? GetConcatenation(gaia_name, custom_name)
                           : custom_name;
  EXPECT_EQ(expected_profile_name,
            GetCache()->GetNameToDisplayOfProfileAtIndex(index2));
  EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(index2));
}

TEST_F(ProfileInfoCacheTest, ConcatenateGaiaNameAndProfileName) {
  // We should only append the profile name to the GAIA name if:
  // - The user has chosen a profile name on purpose.
  // - Two profiles has the sama GAIA name and we need to show it to
  //   clear ambiguity.
  // If one of the two conditions hold, we will show the profile name in this
  // format |GAIA name (Profile local name)|
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kProfileMenuRevamp);
  // Single profile.
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("Person 1"), std::string(),
      base::string16(), false, 0, std::string(), EmptyAccountId());
  int index1 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_1"));
  EXPECT_EQ(ASCIIToUTF16("Person 1"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  GetCache()->SetGAIANameOfProfileAtIndex(index1, ASCIIToUTF16("Patt Smith"));
  EXPECT_EQ(ASCIIToUTF16("Patt Smith"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  GetCache()->SetGAIAGivenNameOfProfileAtIndex(index1, ASCIIToUTF16("Patt"));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));

  // Set a custom profile name.
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index1, false);
  GetCache()->SetLocalProfileNameOfProfileAtIndex(index1, ASCIIToUTF16("Work"));
  EXPECT_EQ(ASCIIToUTF16("Patt (Work)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));

  // Set the profile name to be equal to GAIA name.
  GetCache()->SetLocalProfileNameOfProfileAtIndex(index1, ASCIIToUTF16("patt"));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));

  // Multiple profiles.
  // Add another profile with the same GAIA name and a default profile name.
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("Person 2"), std::string(),
      base::string16(), false, 0, std::string(), EmptyAccountId());
  int index2 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_2"));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Person 2"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index2));

  GetCache()->SetLocalProfileNameOfProfileAtIndex(index1, ASCIIToUTF16("Work"));
  EXPECT_EQ(ASCIIToUTF16("Patt (Work)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Person 2"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index2));

  // A second profile with a different GAIA name should not affect the first
  // profile.
  GetCache()->SetGAIAGivenNameOfProfileAtIndex(index2, ASCIIToUTF16("Olly"));
  EXPECT_EQ(ASCIIToUTF16("Patt (Work)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Olly"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index2));

  // Mark profile name as default.
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index1, true);
  GetCache()->SetLocalProfileNameOfProfileAtIndex(index1,
                                                  ASCIIToUTF16("Person 1"));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Olly"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index2));

  // Add a third profile with the same GAIA name as the first.
  // The two profiles are marked as using default profile names.
  GetCache()->AddProfileToCache(
      GetProfilePath("path_3"), ASCIIToUTF16("Person 3"), std::string(),
      base::string16(), false, 0, std::string(), EmptyAccountId());
  int index3 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_3"));
  GetCache()->SetGAIANameOfProfileAtIndex(index3, ASCIIToUTF16("Patt Smith"));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Patt Smith"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index3));

  // Two profiles with same GAIA name and default profile name.
  // Empty GAIA given name.
  GetCache()->SetGAIANameOfProfileAtIndex(index3, ASCIIToUTF16("Patt"));
  EXPECT_EQ(ASCIIToUTF16("Patt (Person 1)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Patt (Person 3)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index3));
  // Set GAIA given name.
  GetCache()->SetGAIAGivenNameOfProfileAtIndex(index3, ASCIIToUTF16("Patt"));
  EXPECT_EQ(ASCIIToUTF16("Patt (Person 1)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Patt (Person 3)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index3));

  // Customize the profile name for one of the two profiles.
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index3, false);
  GetCache()->SetLocalProfileNameOfProfileAtIndex(index3,
                                                  ASCIIToUTF16("Personal"));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Patt (Personal)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index3));

  // Set one of the profile names to be equal to GAIA name, we should show
  // the profile name even if it is Person n to clear ambiguity.
  GetCache()->SetLocalProfileNameOfProfileAtIndex(index3, ASCIIToUTF16("patt"));
  EXPECT_EQ(ASCIIToUTF16("Patt (Person 1)"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index3));

  // Never show the profile name if it is equal GAIA name.
  GetCache()->SetLocalProfileNameOfProfileAtIndex(index1, ASCIIToUTF16("Patt"));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index1));
  EXPECT_EQ(ASCIIToUTF16("Patt"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index3));
  EXPECT_EQ(ASCIIToUTF16("Olly"),
            GetCache()->GetNameToDisplayOfProfileAtIndex(index2));
}

TEST_F(ProfileInfoCacheTest, DeleteProfile) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, ASCIIToUTF16("name_1"), std::string(),
                                base::string16(), false, 0, std::string(),
                                EmptyAccountId());
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_2 = GetProfilePath("path_2");
  base::string16 name_2 = ASCIIToUTF16("name_2");
  GetCache()->AddProfileToCache(path_2, name_2, std::string(), base::string16(),
                                false, 0, std::string(), EmptyAccountId());
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  GetCache()->DeleteProfileFromCache(path_1);
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_2, GetCache()->GetNameToDisplayOfProfileAtIndex(0));

  GetCache()->DeleteProfileFromCache(path_2);
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
}

TEST_F(ProfileInfoCacheTest, MutateProfile) {
  base::FilePath profile_path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(profile_path_1, ASCIIToUTF16("name_1"),
                                std::string(), base::string16(), false, 0,
                                std::string(), EmptyAccountId());

  base::FilePath profile_path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(profile_path_2, ASCIIToUTF16("name_2"),
                                std::string(), base::string16(), false, 0,
                                std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry1;
  GetCache()->GetProfileAttributesWithPath(profile_path_1, &entry1);
  ProfileAttributesEntry* entry2;
  GetCache()->GetProfileAttributesWithPath(profile_path_2, &entry2);

  base::string16 new_name = ASCIIToUTF16("new_name");
  GetCache()->SetLocalProfileNameOfProfileAtIndex(1, new_name);
  EXPECT_EQ(new_name, GetCache()->GetNameToDisplayOfProfileAtIndex(1));
  EXPECT_NE(new_name, GetCache()->GetNameToDisplayOfProfileAtIndex(0));

  base::string16 new_user_name = ASCIIToUTF16("user_name");
  std::string new_gaia_id = "12345";
  entry2->SetAuthInfo(new_gaia_id, new_user_name, true);
  EXPECT_EQ(new_user_name, entry2->GetUserName());
  EXPECT_EQ(new_gaia_id, GetCache()->GetGAIAIdOfProfileAtIndex(1));
  EXPECT_NE(new_user_name, entry1->GetUserName());

  // Avatar icons not used on Android.
#if !defined(OS_ANDROID)
  const size_t new_icon_index = 3;
  GetCache()->SetAvatarIconOfProfileAtIndex(1, new_icon_index);
  EXPECT_EQ(new_icon_index, GetCache()->GetAvatarIconIndexOfProfileAtIndex(1));

  const size_t wrong_icon_index = profiles::GetDefaultAvatarIconCount() + 1;
  const size_t generic_icon_index = 0;
  GetCache()->SetAvatarIconOfProfileAtIndex(1, wrong_icon_index);
  EXPECT_EQ(generic_icon_index,
            GetCache()->GetAvatarIconIndexOfProfileAtIndex(1));
#endif
}

// Will be removed SOON with ProfileInfoCache tests.
TEST_F(ProfileInfoCacheTest, BackgroundModeStatus) {
  base::FilePath path_1 = GetProfilePath("path_1");
  base::FilePath path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(path_1, ASCIIToUTF16("name_1"), std::string(),
                                base::string16(), false, 0, std::string(),
                                EmptyAccountId());
  GetCache()->AddProfileToCache(path_2, ASCIIToUTF16("name_2"), std::string(),
                                base::string16(), false, 0, std::string(),
                                EmptyAccountId());

  ProfileAttributesEntry* entry_1 = nullptr;
  GetCache()->GetProfileAttributesWithPath(path_1, &entry_1);
  ProfileAttributesEntry* entry_2 = nullptr;
  GetCache()->GetProfileAttributesWithPath(path_2, &entry_2);
  EXPECT_FALSE(entry_1->GetBackgroundStatus());
  EXPECT_FALSE(entry_2->GetBackgroundStatus());

  entry_2->SetBackgroundStatus(true);

  EXPECT_FALSE(entry_1->GetBackgroundStatus());
  EXPECT_TRUE(entry_2->GetBackgroundStatus());

  entry_1->SetBackgroundStatus(true);

  EXPECT_TRUE(entry_1->GetBackgroundStatus());
  EXPECT_TRUE(entry_2->GetBackgroundStatus());

  entry_2->SetBackgroundStatus(false);

  EXPECT_TRUE(entry_1->GetBackgroundStatus());
  EXPECT_FALSE(entry_2->GetBackgroundStatus());
}

TEST_F(ProfileInfoCacheTest, GAIAPicture) {
  const int kDefaultAvatarIndex = 0;
  const int kOtherAvatarIndex = 1;
  const int kGaiaPictureSize = 256;  // Standard size of a Gaia account picture.
  base::FilePath path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(GetProfilePath("path_1"),
                                ASCIIToUTF16("name_1"), std::string(),
                                base::string16(), false, kDefaultAvatarIndex,
                                std::string(), EmptyAccountId());
  GetCache()->AddProfileToCache(path_2, ASCIIToUTF16("name_2"), std::string(),
                                base::string16(), false, kDefaultAvatarIndex,
                                std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry = nullptr;
  GetCache()->GetProfileAttributesWithPath(path_2, &entry);

  // Sanity check.
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(1));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));

  // The profile icon should be the default one.
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(0));
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(1));
  int default_avatar_id =
      profiles::GetDefaultAvatarIconResourceIDAtIndex(kDefaultAvatarIndex);
  const gfx::Image& default_avatar_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(default_avatar_id));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(default_avatar_image, entry->GetAvatarIcon()));

  // Set GAIA picture.
  gfx::Image gaia_image(gfx::test::CreateImage(
      kGaiaPictureSize, kGaiaPictureSize));
  GetCache()->SetGAIAPictureOfProfileAtIndex(1, gaia_image);
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  // Since we're still using the default avatar, the GAIA image should be
  // preferred over the generic avatar image.
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(1));
  EXPECT_TRUE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, entry->GetAvatarIcon()));

  // Set a non-default avatar. This should be preferred over the GAIA image.
  GetCache()->SetAvatarIconOfProfileAtIndex(1, kOtherAvatarIndex);
  GetCache()->SetProfileIsUsingDefaultAvatarAtIndex(1, false);
  EXPECT_FALSE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(1));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
// Avatar icons not used on Android.
#if !defined(OS_ANDROID)
  int other_avatar_id =
      profiles::GetDefaultAvatarIconResourceIDAtIndex(kOtherAvatarIndex);
  const gfx::Image& other_avatar_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(other_avatar_id));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(other_avatar_image, entry->GetAvatarIcon()));
#endif

  // Explicitly setting the GAIA picture should make it preferred again.
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(1, true);
  EXPECT_TRUE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, entry->GetAvatarIcon()));

  // Clearing the IsUsingGAIAPicture flag should result in the generic image
  // being used again.
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(1, false);
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
#if !defined(OS_ANDROID)
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(other_avatar_image, entry->GetAvatarIcon()));
#endif
}

TEST_F(ProfileInfoCacheTest, PersistGAIAPicture) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"), std::string(),
      base::string16(), false, 0, std::string(), EmptyAccountId());
  gfx::Image gaia_image(gfx::test::CreateImage());

  GetCache()->SetGAIAPictureOfProfileAtIndex(0, gaia_image);

  // Make sure everything has completed, and the file has been written to disk.
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(0)));

  ResetCache();
  // Try to get the GAIA picture. This should return NULL until the read from
  // disk is done.
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(gfx::test::AreImagesEqual(
    gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(0)));
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(ProfileInfoCacheTest, SetSupervisedUserId) {
  GetCache()->AddProfileToCache(GetProfilePath("test"), ASCIIToUTF16("Test"),
                                std::string(), base::string16(), false, 0,
                                std::string(), EmptyAccountId());
  EXPECT_FALSE(GetCache()->ProfileIsSupervisedAtIndex(0));

  GetCache()->SetSupervisedUserIdOfProfileAtIndex(
      0, supervised_users::kChildAccountSUID);
  EXPECT_TRUE(GetCache()->ProfileIsSupervisedAtIndex(0));
  EXPECT_EQ(supervised_users::kChildAccountSUID,
            GetCache()->GetSupervisedUserIdOfProfileAtIndex(0));

  ResetCache();
  EXPECT_TRUE(GetCache()->ProfileIsSupervisedAtIndex(0));

  GetCache()->SetSupervisedUserIdOfProfileAtIndex(0, std::string());
  EXPECT_FALSE(GetCache()->ProfileIsSupervisedAtIndex(0));
  EXPECT_EQ("", GetCache()->GetSupervisedUserIdOfProfileAtIndex(0));
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

TEST_F(ProfileInfoCacheTest, EmptyGAIAInfo) {
  base::string16 profile_name = ASCIIToUTF16("name_1");
  int id = profiles::GetDefaultAvatarIconResourceIDAtIndex(0);
  const gfx::Image& profile_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(id));

  base::FilePath profile_path = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                base::string16(), false, 0, std::string(),
                                EmptyAccountId());

  // Set empty GAIA info.
  GetCache()->SetGAIANameOfProfileAtIndex(0, base::string16());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, gfx::Image());
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(0, true);

  // Verify that the profile name and picture are not empty.
  EXPECT_EQ(profile_name, GetCache()->GetNameToDisplayOfProfileAtIndex(0));
  ProfileAttributesEntry* entry = nullptr;
  GetCache()->GetProfileAttributesWithPath(profile_path, &entry);
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, entry->GetAvatarIcon()));
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(ProfileInfoCacheTest, CreateSupervisedTestingProfile) {
  testing_profile_manager_.CreateTestingProfile("default");
  base::string16 supervised_user_name = ASCIIToUTF16("Supervised User");
  testing_profile_manager_.CreateTestingProfile(
      "test1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      supervised_user_name, 0, supervised_users::kChildAccountSUID,
      TestingProfile::TestingFactories());
  for (size_t i = 0; i < GetCache()->GetNumberOfProfiles(); i++) {
    bool is_supervised =
        GetCache()->GetNameToDisplayOfProfileAtIndex(i) == supervised_user_name;
    EXPECT_EQ(is_supervised, GetCache()->ProfileIsSupervisedAtIndex(i));
    std::string supervised_user_id =
        is_supervised ? supervised_users::kChildAccountSUID : "";
    EXPECT_EQ(supervised_user_id,
              GetCache()->GetSupervisedUserIdOfProfileAtIndex(i));
  }

  // Supervised profiles have a custom theme, which needs to be deleted on the
  // FILE thread. Reset the profile manager now so everything is deleted while
  // we still have a FILE thread.
  TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
}
#endif

TEST_F(ProfileInfoCacheTest, AddStubProfile) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  // Add some profiles with and without a '.' in their paths.
  const struct {
    const char* profile_path;
    const char* profile_name;
  } kTestCases[] = {
    { "path.test0", "name_0" },
    { "path_test1", "name_1" },
    { "path.test2", "name_2" },
    { "path_test3", "name_3" },
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    base::string16 profile_name = ASCIIToUTF16(kTestCases[i].profile_name);

    GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                  base::string16(), false, i, "",
                                  EmptyAccountId());

    EXPECT_EQ(profile_path, GetCache()->GetPathOfProfileAtIndex(i));
    EXPECT_EQ(profile_name, GetCache()->GetNameToDisplayOfProfileAtIndex(i));
  }

  ASSERT_EQ(4U, GetCache()->GetNumberOfProfiles());

  // Check that the profiles can be extracted from the local state.
  std::vector<base::string16> names;
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* cache = local_state->GetDictionary(
      prefs::kProfileInfoCache);
  base::string16 name;
  for (base::DictionaryValue::Iterator it(*cache); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* info = NULL;
    it.value().GetAsDictionary(&info);
    info->GetString("name", &name);
    names.push_back(name);
  }

  for (size_t i = 0; i < 4; i++)
    ASSERT_FALSE(names[i].empty());
}

TEST_F(ProfileInfoCacheTest, EntriesInAttributesStorage) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  // Add some profiles with and without a '.' in their paths.
  const struct {
    const char* profile_path;
    const char* profile_name;
  } kTestCases[] = {
    { "path.test0", "name_0" },
    { "path_test1", "name_1" },
    { "path.test2", "name_2" },
    { "path_test3", "name_3" },
  };

  // Profiles are added and removed using all combinations of the old and the
  // new interfaces. The content of |profile_attributes_entries_| in
  // ProfileAttributesStorage is checked after each insert and delete operation.

  // Add profiles.
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    base::string16 profile_name = ASCIIToUTF16(kTestCases[i].profile_name);

    ASSERT_EQ(0u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));

    // Use ProfileInfoCache in profiles 0 and 2, and ProfileAttributesStorage in
    // profiles 1 and 3.
    if (i == 0 || i == 2) {
      GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                    base::string16(), false, i, "",
                                    EmptyAccountId());
    } else {
      GetCache()->AddProfile(profile_path, profile_name, std::string(),
                             base::string16(), false, i, "", EmptyAccountId());
    }

    ASSERT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
    ASSERT_EQ(i + 1, GetCache()->profile_attributes_entries_.size());

    ASSERT_EQ(1u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));
    // TODO(anthonyvd) : check that the entry in |profile_attributes_entries_|
    // is null before GetProfileAttributesWithPath is run. Currently this is
    // impossible to check because GetProfileAttributesWithPath is called during
    // profile creation.

    ProfileAttributesEntry* entry = nullptr;
    GetCache()->GetProfileAttributesWithPath(profile_path, &entry);
    EXPECT_EQ(
        entry,
        GetCache()->profile_attributes_entries_[profile_path.value()].get());
  }

  // Remove profiles.
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    ASSERT_EQ(1u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));

    // Use ProfileInfoCache in profiles 0 and 1, and ProfileAttributesStorage in
    // profiles 2 and 3.
    if (i == 0 || i == 1)
      GetCache()->DeleteProfileFromCache(profile_path);
    else
      GetCache()->RemoveProfile(profile_path);

    ASSERT_EQ(0u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));

    ProfileAttributesEntry* entry = nullptr;
    EXPECT_FALSE(GetCache()->GetProfileAttributesWithPath(profile_path,
                                                          &entry));
    ASSERT_EQ(0u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));
  }
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
TEST_F(ProfileInfoCacheTest, MigrateLegacyProfileNamesAndRecomputeIfNeeded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kProfileMenuRevamp);
  EXPECT_EQ(0U, GetCache()->GetNumberOfProfiles());
  // Mimick a pre-existing Directory with profiles that has legacy profile
  // names.
  base::FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, ASCIIToUTF16("Default Profile"),
                                std::string(), base::string16(), false, 0,
                                std::string(), EmptyAccountId());
  int index1 = GetCache()->GetIndexOfProfileWithPath(path_1);
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index1, true);

  base::FilePath path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(path_2, ASCIIToUTF16("First user"),
                                std::string(), base::string16(), false, 1,
                                std::string(), EmptyAccountId());
  int index2 = GetCache()->GetIndexOfProfileWithPath(path_2);
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index2, true);

  base::string16 name_3 = ASCIIToUTF16("Lemonade");
  base::FilePath path_3 = GetProfilePath("path_3");
  GetCache()->AddProfileToCache(path_3, name_3, std::string(), base::string16(),
                                false, 2, std::string(), EmptyAccountId());
  int index3 = GetCache()->GetIndexOfProfileWithPath(path_3);
  EXPECT_FALSE(GetCache()->ProfileIsUsingDefaultNameAtIndex(index3));
  // Set is using default to true, should migrate "Lemonade" to "Person %n".
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index3, true);

  base::string16 name_4 = ASCIIToUTF16("Batman");
  base::FilePath path_4 = GetProfilePath("path_4");
  GetCache()->AddProfileToCache(path_4, name_4, std::string(), base::string16(),
                                false, 3, std::string(), EmptyAccountId());
  int index4 = GetCache()->GetIndexOfProfileWithPath(path_4);
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index4, true);

  // Should not be migrated.
  base::string16 name_5 = ASCIIToUTF16("Batman");
  base::FilePath path_5 = GetProfilePath("path_5");
  GetCache()->AddProfileToCache(path_5, name_5, std::string(), base::string16(),
                                false, 3, std::string(), EmptyAccountId());

  base::string16 name_6 = ASCIIToUTF16("Person 2");
  base::FilePath path_6 = GetProfilePath("path_6");
  GetCache()->AddProfileToCache(path_6, name_6, std::string(), base::string16(),
                                false, 2, std::string(), EmptyAccountId());

  base::FilePath path_7 = GetProfilePath("path_7");
  base::string16 name_7 = ASCIIToUTF16("Person 3");
  GetCache()->AddProfileToCache(path_7, name_7, std::string(), base::string16(),
                                false, 0, std::string(), EmptyAccountId());

  // Should be renamed to unique Person %n.
  base::FilePath path_8 = GetProfilePath("path_8");
  GetCache()->AddProfileToCache(path_8, ASCIIToUTF16("Person 1"), std::string(),
                                base::string16(), false, 1, std::string(),
                                EmptyAccountId());
  base::FilePath path_9 = GetProfilePath("path_9");
  GetCache()->AddProfileToCache(path_9, ASCIIToUTF16("Person 2"), std::string(),
                                base::string16(), false, 2, std::string(),
                                EmptyAccountId());
  base::FilePath path_10 = GetProfilePath("path_10");
  GetCache()->AddProfileToCache(path_10, ASCIIToUTF16("Person 1"),
                                std::string(), base::string16(), false, 3,
                                std::string(), EmptyAccountId());
  // Should not be migrated.
  base::string16 name_11 = ASCIIToUTF16("Smith");
  base::FilePath path_11 = GetProfilePath("path_11");
  GetCache()->AddProfileToCache(path_11, name_11, std::string(),
                                base::string16(), false, 2, std::string(),
                                EmptyAccountId());
  base::FilePath path_12 = GetProfilePath("path_12");
  GetCache()->AddProfileToCache(path_12, ASCIIToUTF16("Person 2"),
                                std::string(), base::string16(), false, 2,
                                std::string(), EmptyAccountId());

  EXPECT_EQ(12U, GetCache()->GetNumberOfProfiles());

  ResetCache();
  ProfileInfoCache::SetLegacyProfileMigrationForTesting(true);
  GetCache();
  ProfileInfoCache::SetLegacyProfileMigrationForTesting(false);

  EXPECT_EQ(name_5, GetCache()->GetNameToDisplayOfProfileAtIndex(
                        GetCache()->GetIndexOfProfileWithPath(path_5)));
  EXPECT_EQ(name_11, GetCache()->GetNameToDisplayOfProfileAtIndex(
                         GetCache()->GetIndexOfProfileWithPath(path_11)));

  // Legacy profile names like "Default Profile" and "First user" should be
  // migrated to "Person %n" type names, i.e. any permutation of "Person %n".
  std::set<base::string16> expected_profile_names;
  expected_profile_names.insert(ASCIIToUTF16("Person 1"));
  expected_profile_names.insert(ASCIIToUTF16("Person 2"));
  expected_profile_names.insert(ASCIIToUTF16("Person 3"));
  expected_profile_names.insert(ASCIIToUTF16("Person 4"));
  expected_profile_names.insert(ASCIIToUTF16("Person 5"));
  expected_profile_names.insert(ASCIIToUTF16("Person 6"));
  expected_profile_names.insert(ASCIIToUTF16("Person 7"));
  expected_profile_names.insert(ASCIIToUTF16("Person 8"));
  expected_profile_names.insert(ASCIIToUTF16("Person 9"));
  expected_profile_names.insert(ASCIIToUTF16("Person 10"));

  std::set<base::string16> actual_profile_names;
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_1)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_2)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_3)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_4)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_6)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_7)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_8)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_9)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_10)));
  actual_profile_names.insert(GetCache()->GetNameToDisplayOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(path_12)));
  EXPECT_EQ(actual_profile_names, expected_profile_names);
}

TEST_F(ProfileInfoCacheTest, GetGaiaImageForAvatarMenu) {
  // The TestingProfileManager's ProfileInfoCache doesn't download avatars.
  ProfileInfoCache profile_info_cache(
      g_browser_process->local_state(),
      testing_profile_manager_.profile_manager()->user_data_dir());

  base::FilePath profile_path = GetProfilePath("path_1");

  GetCache()->AddProfileToCache(profile_path, ASCIIToUTF16("name_1"),
                                std::string(), base::string16(), false, 0,
                                std::string(), EmptyAccountId());

  gfx::Image gaia_image(gfx::test::CreateImage());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, gaia_image);

  // Make sure everything has completed, and the file has been written to disk.
  content::RunAllTasksUntilIdle();

  // Make sure this profile is using GAIA picture.
  EXPECT_TRUE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(0));

  ResetCache();

  // We need to explicitly set the GAIA usage flag after resetting the cache.
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(0, true);
  EXPECT_TRUE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(0));

  gfx::Image image_loaded;

  // Try to get the GAIA image. For the first time, it triggers an async image
  // load from disk. The load status indicates the image is still being loaded.
  EXPECT_EQ(AvatarMenu::ImageLoadStatus::LOADING,
            AvatarMenu::GetImageForMenuButton(profile_path, &image_loaded));
  EXPECT_FALSE(gfx::test::AreImagesEqual(gaia_image, image_loaded));

  // Wait until the async image load finishes.
  content::RunAllTasksUntilIdle();

  // Since the GAIA image is loaded now, we can get it this time.
  EXPECT_EQ(AvatarMenu::ImageLoadStatus::LOADED,
            AvatarMenu::GetImageForMenuButton(profile_path, &image_loaded));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, image_loaded));
}
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
TEST_F(ProfileInfoCacheTest,
       DontMigrateLegacyProfileNamesWithoutNewAvatarMenu) {
  EXPECT_EQ(0U, GetCache()->GetNumberOfProfiles());

  base::string16 name_1 = ASCIIToUTF16("Default Profile");
  base::FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, name_1, std::string(), base::string16(),
                                false, 0, std::string(), EmptyAccountId());
  base::string16 name_2 = ASCIIToUTF16("First user");
  base::FilePath path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(path_2, name_2, std::string(), base::string16(),
                                false, 1, std::string(), EmptyAccountId());
  base::string16 name_3 = ASCIIToUTF16("Lemonade");
  base::FilePath path_3 = GetProfilePath("path_3");
  GetCache()->AddProfileToCache(path_3, name_3, std::string(), base::string16(),
                                false, 2, std::string(), EmptyAccountId());
  base::string16 name_4 = ASCIIToUTF16("Batman");
  base::FilePath path_4 = GetProfilePath("path_4");
  GetCache()->AddProfileToCache(path_4, name_4, std::string(), base::string16(),
                                false, 3, std::string(), EmptyAccountId());
  EXPECT_EQ(4U, GetCache()->GetNumberOfProfiles());

  ResetCache();

  // Profile names should have been preserved.
  EXPECT_EQ(name_1, GetCache()->GetNameToDisplayOfProfileAtIndex(
                        GetCache()->GetIndexOfProfileWithPath(path_1)));
  EXPECT_EQ(name_2, GetCache()->GetNameToDisplayOfProfileAtIndex(
                        GetCache()->GetIndexOfProfileWithPath(path_2)));
  EXPECT_EQ(name_3, GetCache()->GetNameToDisplayOfProfileAtIndex(
                        GetCache()->GetIndexOfProfileWithPath(path_3)));
  EXPECT_EQ(name_4, GetCache()->GetNameToDisplayOfProfileAtIndex(
                        GetCache()->GetIndexOfProfileWithPath(path_4)));
}
#endif

TEST_F(ProfileInfoCacheTest, RemoveProfileByAccountId) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_1 = GetProfilePath("path_1");
  const AccountId account_id_1(
      AccountId::FromUserEmailGaiaId("email1", "111111"));
  base::string16 name_1 = ASCIIToUTF16("name_1");
  GetCache()->AddProfileToCache(path_1, name_1, account_id_1.GetGaiaId(),
                                UTF8ToUTF16(account_id_1.GetUserEmail()), true,
                                0, std::string(), EmptyAccountId());
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_2 = GetProfilePath("path_2");
  base::string16 name_2 = ASCIIToUTF16("name_2");
  const AccountId account_id_2(
      AccountId::FromUserEmailGaiaId("email2", "222222"));
  GetCache()->AddProfileToCache(path_2, name_2, account_id_2.GetGaiaId(),
                                UTF8ToUTF16(account_id_2.GetUserEmail()), true,
                                0, std::string(), EmptyAccountId());
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_3 = GetProfilePath("path_3");
  base::string16 name_3 = ASCIIToUTF16("name_3");
  const AccountId account_id_3(
      AccountId::FromUserEmailGaiaId("email3", "333333"));
  GetCache()->AddProfileToCache(path_3, name_3, account_id_3.GetGaiaId(),
                                UTF8ToUTF16(account_id_3.GetUserEmail()), false,
                                0, std::string(), EmptyAccountId());
  EXPECT_EQ(3u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_4 = GetProfilePath("path_4");
  base::string16 name_4 = ASCIIToUTF16("name_4");
  const AccountId account_id_4(
      AccountId::FromUserEmailGaiaId("email4", "444444"));
  GetCache()->AddProfileToCache(path_4, name_4, account_id_4.GetGaiaId(),
                                UTF8ToUTF16(account_id_4.GetUserEmail()), false,
                                0, std::string(), EmptyAccountId());
  EXPECT_EQ(4u, GetCache()->GetNumberOfProfiles());

  GetCache()->RemoveProfileByAccountId(account_id_3);
  EXPECT_EQ(3u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_1, GetCache()->GetNameToDisplayOfProfileAtIndex(0));

  GetCache()->RemoveProfileByAccountId(account_id_1);
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_2, GetCache()->GetNameToDisplayOfProfileAtIndex(0));

  // this profile is already deleted.
  GetCache()->RemoveProfileByAccountId(account_id_3);
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_2, GetCache()->GetNameToDisplayOfProfileAtIndex(0));

  // Remove profile by partial match
  GetCache()->RemoveProfileByAccountId(
      AccountId::FromUserEmail(account_id_2.GetUserEmail()));
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_4, GetCache()->GetNameToDisplayOfProfileAtIndex(0));

  // Remove last profile
  GetCache()->RemoveProfileByAccountId(account_id_4);
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
}
