// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache_unittest.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
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

namespace {

size_t GetDefaultAvatarIconResourceIDAtIndex(int index) {
#if defined(OS_WIN)
  return profiles::GetOldDefaultAvatar2xIconResourceIDAtIndex(index);
#else
  return profiles::GetDefaultAvatarIconResourceIDAtIndex(index);
#endif  // defined(OS_WIN)
}

}  //  namespace

ProfileNameVerifierObserver::ProfileNameVerifierObserver(
    TestingProfileManager* testing_profile_manager)
    : testing_profile_manager_(testing_profile_manager) {
  DCHECK(testing_profile_manager_);
}

ProfileNameVerifierObserver::~ProfileNameVerifierObserver() {
}

void ProfileNameVerifierObserver::OnProfileAdded(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry =
      GetCache()->GetProfileAttributesWithPath(profile_path);
  std::u16string profile_name = entry->GetName();
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
    const std::u16string& profile_name) {
  EXPECT_TRUE(profile_names_.find(profile_path) == profile_names_.end());
}

void ProfileNameVerifierObserver::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  ProfileAttributesEntry* entry =
      GetCache()->GetProfileAttributesWithPath(profile_path);
  std::u16string new_profile_name = entry->GetName();
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

std::u16string ProfileInfoCacheTest::GetConcatenation(
    const std::u16string& gaia_name,
    const std::u16string profile_name) {
  std::u16string name_to_display(gaia_name);
  name_to_display.append(u" (");
  name_to_display.append(profile_name);
  name_to_display.append(u")");
  return name_to_display;
}

TEST_F(ProfileInfoCacheTest, AddProfiles) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
  // Avatar icons not used on Android.
#if !defined(OS_ANDROID)
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
#endif

  for (uint32_t i = 0; i < 4; ++i) {
    base::FilePath profile_path =
        GetProfilePath(base::StringPrintf("path_%ud", i));
    std::u16string profile_name =
        ASCIIToUTF16(base::StringPrintf("name_%ud", i));
#if !defined(OS_ANDROID)

    size_t icon_id = GetDefaultAvatarIconResourceIDAtIndex(i);
    const SkBitmap* icon = rb.GetImageNamed(icon_id).ToSkBitmap();

#endif  // !defined(OS_ANDROID)
    std::string supervised_user_id;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    if (i == 3)
      supervised_user_id = supervised_users::kChildAccountSUID;
#endif

    GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                  std::u16string(), false, i,
                                  supervised_user_id, EmptyAccountId());

    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(profile_path);
    entry->SetBackgroundStatus(true);
    std::u16string gaia_name = ASCIIToUTF16(base::StringPrintf("gaia_%ud", i));
    entry->SetGAIAName(gaia_name);

    EXPECT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
    std::u16string expected_profile_name =
        GetConcatenation(gaia_name, profile_name);

    EXPECT_EQ(expected_profile_name, entry->GetName());

    EXPECT_EQ(profile_path, GetCache()->GetPathOfProfileAtIndex(i));
#if !defined(OS_ANDROID)
    const SkBitmap* actual_icon = entry->GetAvatarIcon().ToSkBitmap();
    EXPECT_EQ(icon->width(), actual_icon->width());
    EXPECT_EQ(icon->height(), actual_icon->height());
#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    EXPECT_EQ(i == 3, entry->IsSupervised());
#else
    EXPECT_FALSE(entry->IsSupervised());
    EXPECT_FALSE(entry->IsOmitted());
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
    EXPECT_EQ(supervised_user_id, entry->GetSupervisedUserId());
  }

  // Reset the cache and test the it reloads correctly.
  ResetCache();

  EXPECT_EQ(4u, GetCache()->GetNumberOfProfiles());
  for (uint32_t i = 0; i < 4; ++i) {
    base::FilePath profile_path =
          GetProfilePath(base::StringPrintf("path_%ud", i));
    int index = GetCache()->GetIndexOfProfileWithPath(profile_path);
    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(profile_path);
    std::u16string profile_name =
        ASCIIToUTF16(base::StringPrintf("name_%ud", index));
    std::u16string gaia_name = ASCIIToUTF16(base::StringPrintf("gaia_%ud", i));
    std::u16string expected_profile_name =
        GetConcatenation(gaia_name, profile_name);
    EXPECT_EQ(expected_profile_name, entry->GetName());
#if !defined(OS_ANDROID)
    EXPECT_EQ(i, GetCache()->GetAvatarIconIndexOfProfileAtIndex(index));
#endif
    EXPECT_EQ(true, entry->GetBackgroundStatus());
    EXPECT_EQ(gaia_name, entry->GetGAIAName());
  }
}

TEST_F(ProfileInfoCacheTest, GAIAName) {
  base::FilePath profile_path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(profile_path_1, u"Person 1", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());
  ProfileAttributesEntry* entry_1 =
      GetCache()->GetProfileAttributesWithPath(profile_path_1);
  std::u16string profile_name(u"Person 2");
  base::FilePath profile_path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(GetProfilePath("path_2"), profile_name,
                                std::string(), std::u16string(), false, 0,
                                std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry_2 =
      GetCache()->GetProfileAttributesWithPath(profile_path_2);

  // Sanity check.
  EXPECT_TRUE(entry_1->GetGAIAName().empty());
  EXPECT_TRUE(entry_2->GetGAIAName().empty());

  // Set GAIA name.
  std::u16string gaia_name(u"Pat Smith");
  entry_2->SetGAIAName(gaia_name);
  // Since there is a GAIA name, we use that as a display name.
  EXPECT_TRUE(entry_1->GetGAIAName().empty());
  EXPECT_EQ(gaia_name, entry_2->GetGAIAName());
  EXPECT_EQ(gaia_name, entry_2->GetName());

  std::u16string custom_name(u"Custom name");
  entry_2->SetLocalProfileName(custom_name, false);

  std::u16string expected_profile_name =
      GetConcatenation(gaia_name, custom_name);
  EXPECT_EQ(expected_profile_name, entry_2->GetName());
  EXPECT_EQ(gaia_name, entry_2->GetGAIAName());
}

TEST_F(ProfileInfoCacheTest, ConcatenateGaiaNameAndProfileName) {
  // We should only append the profile name to the GAIA name if:
  // - The user has chosen a profile name on purpose.
  // - Two profiles has the sama GAIA name and we need to show it to
  //   clear ambiguity.
  // If one of the two conditions hold, we will show the profile name in this
  // format |GAIA name (Profile local name)|
  // Single profile.
  GetCache()->AddProfileToCache(GetProfilePath("path_1"), u"Person 1",
                                std::string(), std::u16string(), false, 0,
                                std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry_1 =
      GetCache()->GetProfileAttributesWithPath(GetProfilePath("path_1"));
  EXPECT_EQ(u"Person 1", entry_1->GetName());
  entry_1->SetGAIAName(u"Patt Smith");
  EXPECT_EQ(u"Patt Smith", entry_1->GetName());
  entry_1->SetGAIAGivenName(u"Patt");
  EXPECT_EQ(u"Patt", entry_1->GetName());

  // Set a custom profile name.
  entry_1->SetLocalProfileName(u"Work", false);
  EXPECT_EQ(u"Patt (Work)", entry_1->GetName());

  // Set the profile name to be equal to GAIA name.
  entry_1->SetLocalProfileName(u"patt", false);
  EXPECT_EQ(u"Patt", entry_1->GetName());

  // Multiple profiles.
  // Add another profile with the same GAIA name and a default profile name.
  GetCache()->AddProfileToCache(GetProfilePath("path_2"), u"Person 2",
                                std::string(), std::u16string(), false, 0,
                                std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry_2 =
      GetCache()->GetProfileAttributesWithPath(GetProfilePath("path_2"));
  EXPECT_EQ(u"Patt", entry_1->GetName());
  EXPECT_EQ(u"Person 2", entry_2->GetName());

  entry_1->SetLocalProfileName(u"Work", false);
  EXPECT_EQ(u"Patt (Work)", entry_1->GetName());
  EXPECT_EQ(u"Person 2", entry_2->GetName());

  // A second profile with a different GAIA name should not affect the first
  // profile.
  entry_2->SetGAIAGivenName(u"Olly");
  EXPECT_EQ(u"Patt (Work)", entry_1->GetName());
  EXPECT_EQ(u"Olly", entry_2->GetName());

  // Mark profile name as default.
  entry_1->SetLocalProfileName(u"Person 1", true);
  EXPECT_EQ(u"Patt", entry_1->GetName());
  EXPECT_EQ(u"Olly", entry_2->GetName());

  // Add a third profile with the same GAIA name as the first.
  // The two profiles are marked as using default profile names.
  GetCache()->AddProfileToCache(GetProfilePath("path_3"), u"Person 3",
                                std::string(), std::u16string(), false, 0,
                                std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry_3 =
      GetCache()->GetProfileAttributesWithPath(GetProfilePath("path_3"));
  entry_3->SetGAIAName(u"Patt Smith");
  EXPECT_EQ(u"Patt", entry_1->GetName());
  EXPECT_EQ(u"Patt Smith", entry_3->GetName());

  // Two profiles with same GAIA name and default profile name.
  // Empty GAIA given name.
  entry_3->SetGAIAName(u"Patt");
  EXPECT_EQ(u"Patt (Person 1)", entry_1->GetName());
  EXPECT_EQ(u"Patt (Person 3)", entry_3->GetName());
  // Set GAIA given name.
  entry_3->SetGAIAGivenName(u"Patt");
  EXPECT_EQ(u"Patt (Person 1)", entry_1->GetName());
  EXPECT_EQ(u"Patt (Person 3)", entry_3->GetName());

  // Customize the profile name for one of the two profiles.
  entry_3->SetLocalProfileName(u"Personal", false);
  EXPECT_EQ(u"Patt", entry_1->GetName());
  EXPECT_EQ(u"Patt (Personal)", entry_3->GetName());

  // Set one of the profile names to be equal to GAIA name, we should show
  // the profile name even if it is Person n to clear ambiguity.
  entry_3->SetLocalProfileName(u"patt", false);
  EXPECT_EQ(u"Patt (Person 1)", entry_1->GetName());
  EXPECT_EQ(u"Patt", entry_3->GetName());

  // Never show the profile name if it is equal GAIA name.
  entry_1->SetLocalProfileName(u"Patt", false);
  EXPECT_EQ(u"Patt", entry_1->GetName());
  EXPECT_EQ(u"Patt", entry_3->GetName());
  EXPECT_EQ(u"Olly", entry_2->GetName());
}

TEST_F(ProfileInfoCacheTest, DeleteProfile) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, u"name_1", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_2 = GetProfilePath("path_2");
  std::u16string name_2 = u"name_2";
  GetCache()->AddProfileToCache(path_2, name_2, std::string(), std::u16string(),
                                false, 0, std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry =
      GetCache()->GetProfileAttributesWithPath(path_2);
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  GetCache()->DeleteProfileFromCache(path_1);
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_2, entry->GetName());

  GetCache()->DeleteProfileFromCache(path_2);
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
}

TEST_F(ProfileInfoCacheTest, MutateProfile) {
  base::FilePath profile_path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(profile_path_1, u"name_1", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());

  base::FilePath profile_path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(profile_path_2, u"name_2", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());
  ProfileAttributesEntry* entry_1 =
      GetCache()->GetProfileAttributesWithPath(profile_path_1);
  ProfileAttributesEntry* entry_2 =
      GetCache()->GetProfileAttributesWithPath(profile_path_2);

  std::u16string new_name = u"new_name";
  entry_2->SetLocalProfileName(new_name, false);
  EXPECT_EQ(new_name, entry_2->GetName());
  EXPECT_NE(new_name, entry_1->GetName());

  std::u16string new_user_name = u"user_name";
  std::string new_gaia_id = "12345";
  entry_2->SetAuthInfo(new_gaia_id, new_user_name, true);
  EXPECT_EQ(new_user_name, entry_2->GetUserName());
  EXPECT_EQ(new_gaia_id, entry_2->GetGAIAId());
  EXPECT_NE(new_user_name, entry_1->GetUserName());

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
  GetCache()->AddProfileToCache(path_1, u"name_1", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());
  GetCache()->AddProfileToCache(path_2, u"name_2", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());

  ProfileAttributesEntry* entry_1 =
      GetCache()->GetProfileAttributesWithPath(path_1);
  ProfileAttributesEntry* entry_2 =
      GetCache()->GetProfileAttributesWithPath(path_2);
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
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), u"name_1", std::string(), std::u16string(),
      false, kDefaultAvatarIndex, std::string(), EmptyAccountId());
  GetCache()->AddProfileToCache(path_2, u"name_2", std::string(),
                                std::u16string(), false, kDefaultAvatarIndex,
                                std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry =
      GetCache()->GetProfileAttributesWithPath(path_2);

  // Sanity check.
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(1));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));

  // The profile icon should be the default one.
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(0));
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(1));
  size_t default_avatar_id =
      GetDefaultAvatarIconResourceIDAtIndex(kDefaultAvatarIndex);
  const gfx::Image& default_avatar_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(default_avatar_id));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(default_avatar_image, entry->GetAvatarIcon()));

  // Set GAIA picture.
  gfx::Image gaia_image(gfx::test::CreateImage(
      kGaiaPictureSize, kGaiaPictureSize));
  GetCache()->SetGAIAPictureOfProfileAtIndex(1, "GAIA_IMAGE_URL_WITH_SIZE_1",
                                             gaia_image);
  EXPECT_EQ(nullptr, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
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

  size_t other_avatar_id =
      GetDefaultAvatarIconResourceIDAtIndex(kOtherAvatarIndex);
  const gfx::Image& other_avatar_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(other_avatar_id));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(other_avatar_image, entry->GetAvatarIcon()));
#endif  // !defined(OS_ANDROID)

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
  GetCache()->AddProfileToCache(GetProfilePath("path_1"), u"name_1",
                                std::string(), std::u16string(), false, 0,
                                std::string(), EmptyAccountId());
  gfx::Image gaia_image(gfx::test::CreateImage());

  GetCache()->SetGAIAPictureOfProfileAtIndex(0, "GAIA_IMAGE_URL_WITH_SIZE_0",
                                             gaia_image);

  // Make sure everything has completed, and the file has been written to disk.
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(
      GetCache()->GetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(0),
      "GAIA_IMAGE_URL_WITH_SIZE_0");
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(0)));

  ResetCache();
  // Try to get the GAIA picture. This should return NULL until the read from
  // disk is done.
  EXPECT_EQ(nullptr, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_EQ(
      GetCache()->GetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(0),
      "GAIA_IMAGE_URL_WITH_SIZE_0");
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(gfx::test::AreImagesEqual(
    gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(0)));
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(ProfileInfoCacheTest, SetSupervisedUserId) {
  base::FilePath profile_path = GetProfilePath("test");
  GetCache()->AddProfileToCache(profile_path, u"Test", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());
  ProfileAttributesEntry* entry =
      GetCache()->GetProfileAttributesWithPath(profile_path);
  EXPECT_FALSE(entry->IsSupervised());

  entry->SetSupervisedUserId(supervised_users::kChildAccountSUID);
  EXPECT_TRUE(entry->IsSupervised());
  EXPECT_EQ(supervised_users::kChildAccountSUID, entry->GetSupervisedUserId());

  ResetCache();
  entry = GetCache()->GetProfileAttributesWithPath(profile_path);
  EXPECT_TRUE(entry->IsSupervised());

  entry->SetSupervisedUserId(std::string());
  EXPECT_FALSE(entry->IsSupervised());
  EXPECT_EQ("", entry->GetSupervisedUserId());
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

TEST_F(ProfileInfoCacheTest, EmptyGAIAInfo) {
  std::u16string profile_name = u"name_1";
  size_t id = GetDefaultAvatarIconResourceIDAtIndex(0);
  const gfx::Image& profile_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(id));

  base::FilePath profile_path = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());

  ProfileAttributesEntry* entry =
      GetCache()->GetProfileAttributesWithPath(profile_path);

  gfx::Image gaia_image(gfx::test::CreateImage());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, "GAIA_IMAGE_URL_WITH_SIZE_0",
                                             gaia_image);

  // Make sure everything has completed, and the file has been written to disk.
  content::RunAllTasksUntilIdle();

  // Set empty GAIA info.
  entry->SetGAIAName(std::u16string());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, std::string(), gfx::Image());
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(0, true);

  EXPECT_TRUE(GetCache()
                  ->GetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(0)
                  .empty());

  // Verify that the profile name and picture are not empty.
  EXPECT_EQ(profile_name, entry->GetName());
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, entry->GetAvatarIcon()));
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(ProfileInfoCacheTest, CreateSupervisedTestingProfile) {
  base::FilePath path_1 =
      testing_profile_manager_.CreateTestingProfile("default")->GetPath();
  std::u16string supervised_user_name = u"Supervised User";
  base::FilePath path_2 =
      testing_profile_manager_
          .CreateTestingProfile(
              "test1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
              supervised_user_name, 0, supervised_users::kChildAccountSUID,
              TestingProfile::TestingFactories())
          ->GetPath();
  base::FilePath profile_path[] = {path_1, path_2};
  for (const base::FilePath& path : profile_path) {
    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(path);
    bool is_supervised = entry->GetName() == supervised_user_name;
    EXPECT_EQ(is_supervised, entry->IsSupervised());
    std::string supervised_user_id =
        is_supervised ? supervised_users::kChildAccountSUID : "";
    EXPECT_EQ(supervised_user_id, entry->GetSupervisedUserId());
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
    std::u16string profile_name = ASCIIToUTF16(kTestCases[i].profile_name);

    GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                  std::u16string(), false, i, "",
                                  EmptyAccountId());
    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(profile_path);
    EXPECT_TRUE(entry);
    EXPECT_EQ(profile_name, entry->GetName());
  }

  ASSERT_EQ(4U, GetCache()->GetNumberOfProfiles());

  // Check that the profiles can be extracted from the local state.
  std::vector<std::u16string> names;
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* cache = local_state->GetDictionary(
      prefs::kProfileInfoCache);
  std::u16string name;
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
    std::u16string profile_name = ASCIIToUTF16(kTestCases[i].profile_name);

    ASSERT_EQ(0u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));

    // Use ProfileInfoCache in profiles 0 and 2, and ProfileAttributesStorage in
    // profiles 1 and 3.
    if (i == 0 || i == 2) {
      GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                    std::u16string(), false, i, "",
                                    EmptyAccountId());
    } else {
      GetCache()->AddProfile(profile_path, profile_name, std::string(),
                             std::u16string(), false, i, "", EmptyAccountId());
    }

    ASSERT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
    ASSERT_EQ(i + 1, GetCache()->profile_attributes_entries_.size());

    ASSERT_EQ(1u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));
    // TODO(anthonyvd) : check that the entry in |profile_attributes_entries_|
    // is null before GetProfileAttributesWithPath is run. Currently this is
    // impossible to check because GetProfileAttributesWithPath is called during
    // profile creation.

    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(profile_path);
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

    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(profile_path);
    EXPECT_EQ(entry, nullptr);
    ASSERT_EQ(0u, GetCache()->profile_attributes_entries_.count(
                      profile_path.value()));
  }
}

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileInfoCacheTest, MigrateLegacyProfileNamesAndRecomputeIfNeeded) {
  EXPECT_EQ(0U, GetCache()->GetNumberOfProfiles());
  // Mimick a pre-existing Directory with profiles that has legacy profile
  // names.
  const struct {
    const char* profile_path;
    const char* profile_name;
    bool is_using_default_name;
  } kTestCases[] = {
      {"path_1", "Default Profile", true}, {"path_2", "First user", true},
      {"path_3", "Lemonade", true},        {"path_4", "Batman", true},
      {"path_5", "Batman", false},         {"path_6", "Person 2", true},
      {"path_7", "Person 3", true},        {"path_8", "Person 1", true},
      {"path_9", "Person 2", true},        {"path_10", "Person 1", true},
      {"path_11", "Smith", false},         {"path_12", "Person 2", true}};

  ProfileAttributesEntry* entry = nullptr;
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    std::u16string profile_name = ASCIIToUTF16(kTestCases[i].profile_name);

    GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                  std::u16string(), false, i, "",
                                  EmptyAccountId());
    entry = GetCache()->GetProfileAttributesWithPath(profile_path);
    EXPECT_TRUE(entry);
    entry->SetIsUsingDefaultName(kTestCases[i].is_using_default_name);
  }

  EXPECT_EQ(12U, GetCache()->GetNumberOfProfiles());

  ResetCache();
  ProfileInfoCache::SetLegacyProfileMigrationForTesting(true);
  GetCache();
  ProfileInfoCache::SetLegacyProfileMigrationForTesting(false);

  entry = GetCache()->GetProfileAttributesWithPath(
      GetProfilePath(kTestCases[4].profile_path));
  EXPECT_EQ(ASCIIToUTF16(kTestCases[4].profile_name), entry->GetName());
  entry = GetCache()->GetProfileAttributesWithPath(
      GetProfilePath(kTestCases[10].profile_path));
  EXPECT_EQ(ASCIIToUTF16(kTestCases[10].profile_name), entry->GetName());

  // Legacy profile names like "Default Profile" and "First user" should be
  // migrated to "Person %n" type names, i.e. any permutation of "Person %n".
  std::set<std::u16string> expected_profile_names{
      u"Person 1", u"Person 2", u"Person 3", u"Person 4", u"Person 5",
      u"Person 6", u"Person 7", u"Person 8", u"Person 9", u"Person 10"};

  const char* profile_path[] = {
      kTestCases[0].profile_path, kTestCases[1].profile_path,
      kTestCases[2].profile_path, kTestCases[3].profile_path,
      kTestCases[5].profile_path, kTestCases[6].profile_path,
      kTestCases[7].profile_path, kTestCases[8].profile_path,
      kTestCases[9].profile_path, kTestCases[11].profile_path};

  std::set<std::u16string> actual_profile_names;
  for (auto* path : profile_path) {
    entry = GetCache()->GetProfileAttributesWithPath(GetProfilePath(path));
    actual_profile_names.insert(entry->GetName());
  }
  EXPECT_EQ(actual_profile_names, expected_profile_names);
}

TEST_F(ProfileInfoCacheTest, GetGaiaImageForAvatarMenu) {
  // The TestingProfileManager's ProfileInfoCache doesn't download avatars.
  ProfileInfoCache profile_info_cache(
      g_browser_process->local_state(),
      testing_profile_manager_.profile_manager()->user_data_dir());

  base::FilePath profile_path = GetProfilePath("path_1");

  GetCache()->AddProfileToCache(profile_path, u"name_1", std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());

  gfx::Image gaia_image(gfx::test::CreateImage());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, "GAIA_IMAGE_URL_WITH_SIZE_0",
                                             gaia_image);

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
  constexpr int kArbitraryPreferredSize = 96;
  EXPECT_EQ(AvatarMenu::ImageLoadStatus::LOADING,
            AvatarMenu::GetImageForMenuButton(profile_path, &image_loaded,
                                              kArbitraryPreferredSize));
  EXPECT_FALSE(gfx::test::AreImagesEqual(gaia_image, image_loaded));

  // Wait until the async image load finishes.
  content::RunAllTasksUntilIdle();

  // Since the GAIA image is loaded now, we can get it this time.
  EXPECT_EQ(AvatarMenu::ImageLoadStatus::LOADED,
            AvatarMenu::GetImageForMenuButton(profile_path, &image_loaded,
                                              kArbitraryPreferredSize));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, image_loaded));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_ANDROID)
TEST_F(ProfileInfoCacheTest,
       DontMigrateLegacyProfileNamesWithoutNewAvatarMenu) {
  EXPECT_EQ(0U, GetCache()->GetNumberOfProfiles());

  const struct {
    const char* profile_path;
    const char* profile_name;
  } kTestCases[] = {{"path_1", "Default Profile"},
                    {"path_2", "First user"},
                    {"path_3", "Lemonade"},
                    {"path_4", "Batman"}};

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    std::u16string profile_name = ASCIIToUTF16(kTestCases[i].profile_name);
    GetCache()->AddProfileToCache(profile_path, profile_name, std::string(),
                                  std::u16string(), false, i, "",
                                  EmptyAccountId());
    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(profile_path);
    EXPECT_TRUE(entry);
    entry->SetIsUsingDefaultName(true);
  }
  EXPECT_EQ(4U, GetCache()->GetNumberOfProfiles());

  ResetCache();

  // Profile names should have been preserved.
  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    std::u16string profile_name = ASCIIToUTF16(kTestCases[i].profile_name);
    ProfileAttributesEntry* entry =
        GetCache()->GetProfileAttributesWithPath(profile_path);
    EXPECT_TRUE(entry);
    EXPECT_EQ(profile_name, entry->GetName());
  }
}
#endif

TEST_F(ProfileInfoCacheTest, RemoveProfileByAccountId) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
  const struct {
    const char* profile_path;
    const char* profile_name;
    AccountId account_id;
    bool is_consented_primary_account;
  } kTestCases[] = {
      {"path_1", "name_1", AccountId::FromUserEmailGaiaId("email1", "111111"),
       true},
      {"path_2", "name_3", AccountId::FromUserEmailGaiaId("email2", "222222"),
       true},
      {"path_3", "name_3", AccountId::FromUserEmailGaiaId("email3", "333333"),
       false},
      {"path_4", "name_4", AccountId::FromUserEmailGaiaId("email4", "444444"),
       false}};

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    std::u16string profile_name = ASCIIToUTF16(kTestCases[i].profile_name);
    GetCache()->AddProfileToCache(
        profile_path, profile_name, kTestCases[i].account_id.GetGaiaId(),
        UTF8ToUTF16(kTestCases[i].account_id.GetUserEmail()),
        kTestCases[i].is_consented_primary_account, 0, std::string(),
        EmptyAccountId());
    EXPECT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
  }

  GetCache()->RemoveProfileByAccountId(kTestCases[2].account_id);
  EXPECT_EQ(3u, GetCache()->GetNumberOfProfiles());

  GetCache()->RemoveProfileByAccountId(kTestCases[0].account_id);
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  // this profile is already deleted.
  GetCache()->RemoveProfileByAccountId(kTestCases[2].account_id);
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  // Remove profile by partial match
  GetCache()->RemoveProfileByAccountId(
      AccountId::FromUserEmail(kTestCases[1].account_id.GetUserEmail()));
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());

  // Remove last profile
  GetCache()->RemoveProfileByAccountId(kTestCases[3].account_id);
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
}

#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_WIN)
// Checks that ProfileInfoCache doesn't crash when ProfileAttributesEntry
// initialization modifies the cache entry.
// This is a regression test for https://crbug.com/1180497.
TEST_F(ProfileInfoCacheTest, ModifiyEntryWhileInitializing) {
  base::FilePath profile_path = GetProfilePath("test");
  AccountId account_id = AccountId::FromUserEmailGaiaId("email", "111111");
  GetCache()->AddProfileToCache(profile_path, u"Test", account_id.GetGaiaId(),
                                UTF8ToUTF16(account_id.GetUserEmail()), false,
                                0, std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry =
      GetCache()->GetProfileAttributesWithPath(profile_path);
  // Set up the state so that ProfileAttributesEntry::Initialize() will modify
  // the cache entry.
  entry->SetIsSigninRequired(true);
  // Reinitialize ProfileInfoCache.
  ResetCache();
  GetCache();  // Should not crash.

  // The IsSigninRequired attribute should be cleaned up.
  entry = GetCache()->GetProfileAttributesWithPath(profile_path);
  EXPECT_FALSE(entry->IsSigninRequired());
}
#endif

TEST_F(ProfileInfoCacheTest, ProfileNamesOnInit) {
  // Set up the cache with two profiles having the same GAIA given name.
  // The second profile also has a profile name matching the GAIA given name.
  std::u16string kDefaultProfileName = u"Person 1";
  std::u16string kCommonName = u"Joe";

  // Create and initialize the first profile.
  base::FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, kDefaultProfileName, std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());
  ProfileAttributesEntry* entry_1 =
      GetCache()->GetProfileAttributesWithPath(path_1);
  entry_1->SetGAIAGivenName(kCommonName);
  EXPECT_EQ(entry_1->GetName(), kCommonName);

  // Create and initialize the second profile.
  base::FilePath path_2 = GetProfilePath("path_2");
  GetCache()->AddProfileToCache(path_2, kCommonName, std::string(),
                                std::u16string(), false, 0, std::string(),
                                EmptyAccountId());
  ProfileAttributesEntry* entry_2 =
      GetCache()->GetProfileAttributesWithPath(path_2);
  entry_2->SetGAIAGivenName(kCommonName);
  EXPECT_EQ(entry_2->GetName(), kCommonName);

  // The first profile name should be modified.
  EXPECT_EQ(entry_1->GetName(),
            GetConcatenation(kCommonName, kDefaultProfileName));

  // Reset cache to test profile names set on initialization.
  ResetCache();
  entry_1 = GetCache()->GetProfileAttributesWithPath(path_1);
  entry_2 = GetCache()->GetProfileAttributesWithPath(path_2);

  // Freshly initialized entries should not report name changes.
  EXPECT_FALSE(entry_1->HasProfileNameChanged());
  EXPECT_FALSE(entry_2->HasProfileNameChanged());

  EXPECT_EQ(entry_1->GetName(),
            GetConcatenation(kCommonName, kDefaultProfileName));
  EXPECT_EQ(entry_2->GetName(), kCommonName);
}
