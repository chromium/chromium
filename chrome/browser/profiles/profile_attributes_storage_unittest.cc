// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/profiles/profile_attributes_storage.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <unordered_set>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/profile_metrics/state.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/native_theme/native_theme.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#endif
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

using ::testing::Mock;
using ::testing::_;

namespace {
// The ProfileMetadataEntry accessors aren't just plain old accessors to local
// members so they'll be tested. The following helpers will make the testing
// code less verbose.
#define TEST_ACCESSORS(entry_type, entry, member, first_value, second_value) \
    TestAccessors(&entry, \
                  &entry_type::Get ## member, \
                  &entry_type::Set ## member, \
                  first_value, \
                  second_value);

#define TEST_STRING16_ACCESSORS(entry_type, entry, member)   \
  TEST_ACCESSORS(entry_type, entry, member,                  \
                 std::u16string(u"first_" #member "_value"), \
                 std::u16string(u"second_" #member "_value"));

#define TEST_STRING_ACCESSORS(entry_type, entry, member) \
    TEST_ACCESSORS(entry_type, entry, member, \
        std::string("first_" #member "_value"), \
        std::string("second_" #member "_value"));

#define TEST_BOOL_ACCESSORS(entry_type, entry, member)                       \
  TestAccessors(&entry, &entry_type::member, &entry_type::Set##member, true, \
                false);

template<typename TValue, typename TGetter, typename TSetter>
void TestAccessors(ProfileAttributesEntry** entry,
    TGetter getter_func,
    TSetter setter_func,
    TValue first_value,
    TValue second_value) {
  (*entry->*setter_func)(first_value);
  EXPECT_EQ(first_value, (*entry->*getter_func)());
  (*entry->*setter_func)(second_value);
  EXPECT_EQ(second_value, (*entry->*getter_func)());
}

void VerifyInitialValues(ProfileAttributesEntry* entry,
                         const base::FilePath& profile_path,
                         const std::u16string& profile_name,
                         const std::string& gaia_id,
                         const std::u16string& user_name,
                         bool is_consented_primary_account,
                         size_t icon_index,
                         const std::string& supervised_user_id,
                         bool is_ephemeral,
                         bool is_omitted,
                         bool is_signed_in_with_credential_provider) {
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(profile_path, entry->GetPath());
  EXPECT_EQ(profile_name, entry->GetName());
  EXPECT_EQ(gaia_id, entry->GetGAIAId());
  EXPECT_EQ(user_name, entry->GetUserName());
  EXPECT_EQ(is_consented_primary_account, entry->IsAuthenticated());
  EXPECT_EQ(icon_index, entry->GetAvatarIconIndex());
  EXPECT_EQ(supervised_user_id, entry->GetSupervisedUserId());
  EXPECT_EQ(is_ephemeral, entry->IsEphemeral());
  EXPECT_EQ(is_omitted, entry->IsOmitted());
  EXPECT_EQ(is_signed_in_with_credential_provider,
            entry->IsSignedInWithCredentialProvider());
  EXPECT_EQ(std::string(), entry->GetHostedDomain());
}

class ProfileAttributesTestObserver
    : public ProfileAttributesStorage::Observer {
 public:
  MOCK_METHOD1(OnProfileAdded, void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileWillBeRemoved,
               void(const base::FilePath& profile_path));
  MOCK_METHOD2(OnProfileWasRemoved,
               void(const base::FilePath& profile_path,
                    const std::u16string& profile_name));
  MOCK_METHOD2(OnProfileNameChanged,
               void(const base::FilePath& profile_path,
                    const std::u16string& old_profile_name));
  MOCK_METHOD1(OnProfileAuthInfoChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileAvatarChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileHighResAvatarLoaded,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileSigninRequiredChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileSupervisedUserIdChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileIsOmittedChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileThemeColorsChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileHostedDomainChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileUserManagementAcceptanceChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileManagementEnrollmentTokenChanged,
               void(const base::FilePath& profile_path));
  MOCK_METHOD1(OnProfileManagementIdChanged,
               void(const base::FilePath& profile_path));
};

size_t GetDefaultAvatarIconResourceIDAtIndex(int index) {
#if BUILDFLAG(IS_WIN)
  return profiles::GetOldDefaultAvatar2xIconResourceIDAtIndex(index);
#else
  return profiles::GetDefaultAvatarIconResourceIDAtIndex(index);
#endif  // BUILDFLAG(IS_WIN)
}

std::u16string ConcatenateGaiaAndProfileNames(
    const std::u16string& gaia_name,
    const std::u16string& profile_name) {
  return base::StrCat({gaia_name, u" (", profile_name, u")"});
}

}  // namespace

class ProfileAttributesStorageTest : public testing::Test {
 public:
  ProfileAttributesStorageTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
#if BUILDFLAG(IS_CHROMEOS)
    scoped_cros_settings_test_helper_ =
        std::make_unique<ash::ScopedCrosSettingsTestHelper>();
#endif
  }

  ~ProfileAttributesStorageTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    VerifyAndResetCallExpectations();
    EnableObserver();
  }

  base::FilePath GetProfilePath(const std::string& base_name) {
    return testing_profile_manager_.profile_manager()->user_data_dir().
        AppendASCII(base_name);
  }

  void VerifyAndResetCallExpectations() {
    Mock::VerifyAndClear(&observer_);
    EXPECT_CALL(observer_, OnProfileAdded(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileWillBeRemoved(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileWasRemoved(_, _)).Times(0);
    EXPECT_CALL(observer_, OnProfileNameChanged(_, _)).Times(0);
    EXPECT_CALL(observer_, OnProfileAuthInfoChanged(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileAvatarChanged(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileHighResAvatarLoaded(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileSigninRequiredChanged(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileSupervisedUserIdChanged(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileIsOmittedChanged(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileThemeColorsChanged(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileHostedDomainChanged(_)).Times(0);
    EXPECT_CALL(observer_, OnProfileManagementEnrollmentTokenChanged(_))
        .Times(0);
    EXPECT_CALL(observer_, OnProfileManagementIdChanged(_)).Times(0);
  }

  void EnableObserver() { scoped_observation_.Observe(storage()); }
  void DisableObserver() { scoped_observation_.Reset(); }

  void AddCallExpectationsForRemoveProfile(size_t profile_number) {
    base::FilePath profile_path = GetProfilePath(
        base::StringPrintf("testing_profile_path%" PRIuS, profile_number));
    std::u16string profile_name = base::ASCIIToUTF16(
        base::StringPrintf("testing_profile_name%" PRIuS, profile_number));

    ::testing::InSequence dummy;
    EXPECT_CALL(observer(), OnProfileWillBeRemoved(profile_path)).Times(1);
    EXPECT_CALL(observer(), OnProfileWasRemoved(profile_path, profile_name))
        .Times(1);
  }

  ProfileAttributesStorage* storage() {
    return testing_profile_manager_.profile_attributes_storage();
  }

  ProfileAttributesTestObserver& observer() { return observer_; }

  void AddTestingProfile() {
    size_t number_of_profiles = storage()->GetNumberOfProfiles();

    base::FilePath profile_path = GetProfilePath(
        base::StringPrintf("testing_profile_path%" PRIuS, number_of_profiles));

    EXPECT_CALL(observer(), OnProfileAdded(profile_path))
        .Times(1)
        .RetiresOnSaturation();

    ProfileAttributesInitParams params;
    params.profile_path = profile_path;
    params.profile_name = base::ASCIIToUTF16(
        base::StringPrintf("testing_profile_name%" PRIuS, number_of_profiles));
    params.gaia_id =
        base::StringPrintf("testing_profile_gaia%" PRIuS, number_of_profiles);
    params.user_name = base::ASCIIToUTF16(
        base::StringPrintf("testing_profile_user%" PRIuS, number_of_profiles));
    params.is_consented_primary_account = true;
    params.icon_index = number_of_profiles;
    storage()->AddProfile(std::move(params));

    EXPECT_EQ(number_of_profiles + 1, storage()->GetNumberOfProfiles());
  }

  void ResetProfileAttributesStorage() {
    bool was_observing = scoped_observation_.IsObserving();
    DisableObserver();
    testing_profile_manager_.DeleteProfileAttributesStorage();
    // Restore observation if there was any.
    if (was_observing)
      EnableObserver();
  }

  void AddSimpleTestingProfileWithName(const std::u16string& profile_name) {
    ProfileAttributesInitParams params;
    params.profile_path = GetProfilePath(base::UTF16ToASCII(profile_name));
    params.profile_name = profile_name;
    storage()->AddProfile(std::move(params));
  }

  const std::vector<std::string> EntriesToKeys(
      const std::vector<ProfileAttributesEntry*>& entries) {
    std::vector<std::string> keys;
    keys.reserve(entries.size());
    for (const ProfileAttributesEntry* entry : entries) {
      keys.push_back(storage()->StorageKeyFromProfilePath(entry->GetPath()));
    }
    return keys;
  }

  TestingProfileManager& testing_profile_manager() {
    return testing_profile_manager_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::ScopedCrosSettingsTestHelper>
      scoped_cros_settings_test_helper_;
#endif
  TestingProfileManager testing_profile_manager_;
  ProfileAttributesTestObserver observer_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      scoped_observation_{&observer_};
};

TEST_F(ProfileAttributesStorageTest, ProfileNotFound) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  ASSERT_EQ(storage()->GetProfileAttributesWithPath(
                GetProfilePath("testing_profile_path0")),
            nullptr);

  AddTestingProfile();
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());

  ASSERT_NE(storage()->GetProfileAttributesWithPath(
                GetProfilePath("testing_profile_path0")),
            nullptr);
  ASSERT_EQ(storage()->GetProfileAttributesWithPath(
                GetProfilePath("testing_profile_path1")),
            nullptr);
}

TEST_F(ProfileAttributesStorageTest, AddProfile) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  EXPECT_CALL(observer(), OnProfileAdded(GetProfilePath("new_profile_path_1")))
      .Times(1);

  ProfileAttributesInitParams params;
  params.profile_path = GetProfilePath("new_profile_path_1");
  params.profile_name = u"new_profile_name_1";
  params.gaia_id = "new_profile_gaia_1";
  params.user_name = u"new_profile_username_1";
  params.is_consented_primary_account = true;
  params.icon_index = 1;
  storage()->AddProfile(std::move(params));

  VerifyAndResetCallExpectations();
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("new_profile_path_1"));
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(u"new_profile_name_1", entry->GetName());
}

TEST_F(ProfileAttributesStorageTest, AddProfiles) {
  DisableObserver();  // This test doesn't test observers.

  EXPECT_EQ(0u, storage()->GetNumberOfProfiles());
  // Avatar icons not used on Android.
#if !BUILDFLAG(IS_ANDROID)
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
#endif

  for (size_t i = 0; i < 4u; ++i) {
    base::FilePath profile_path =
        GetProfilePath(base::StringPrintf("path_%zu", i));
    std::u16string profile_name =
        base::ASCIIToUTF16(base::StringPrintf("name_%zu", i));
#if !BUILDFLAG(IS_ANDROID)

    size_t icon_id = GetDefaultAvatarIconResourceIDAtIndex(i);
    const SkBitmap* icon = rb.GetImageNamed(icon_id).ToSkBitmap();

#endif  // !BUILDFLAG(IS_ANDROID)
    std::string supervised_user_id;
    if (i == 3u)
      supervised_user_id = supervised_user::kChildAccountSUID;

    ProfileAttributesInitParams params;
    params.profile_path = profile_path;
    params.profile_name = profile_name;
    params.icon_index = i;
    params.supervised_user_id = supervised_user_id;
    storage()->AddProfile(std::move(params));

    ProfileAttributesEntry* entry =
        storage()->GetProfileAttributesWithPath(profile_path);
    entry->SetBackgroundStatus(true);
    std::u16string gaia_name =
        base::ASCIIToUTF16(base::StringPrintf("gaia_%zu", i));
    entry->SetGAIAName(gaia_name);

    EXPECT_EQ(i + 1, storage()->GetNumberOfProfiles());
    std::u16string expected_profile_name =
        ConcatenateGaiaAndProfileNames(gaia_name, profile_name);

    EXPECT_EQ(expected_profile_name, entry->GetName());

    EXPECT_EQ(profile_path, entry->GetPath());
#if !BUILDFLAG(IS_ANDROID)
    const SkBitmap* actual_icon = entry->GetAvatarIcon().ToSkBitmap();
    EXPECT_EQ(icon->width(), actual_icon->width());
    EXPECT_EQ(icon->height(), actual_icon->height());
#endif
    EXPECT_EQ(i == 3u, entry->IsSupervised());
    EXPECT_EQ(supervised_user_id, entry->GetSupervisedUserId());
  }

  // Reset the storage and test the it reloads correctly.
  ResetProfileAttributesStorage();

  EXPECT_EQ(4u, storage()->GetNumberOfProfiles());
  for (size_t i = 0; i < 4u; ++i) {
    base::FilePath profile_path =
        GetProfilePath(base::StringPrintf("path_%zu", i));
    ProfileAttributesEntry* entry =
        storage()->GetProfileAttributesWithPath(profile_path);
    std::u16string profile_name =
        base::ASCIIToUTF16(base::StringPrintf("name_%zu", i));
    std::u16string gaia_name =
        base::ASCIIToUTF16(base::StringPrintf("gaia_%zu", i));
    std::u16string expected_profile_name =
        ConcatenateGaiaAndProfileNames(gaia_name, profile_name);
    EXPECT_EQ(expected_profile_name, entry->GetName());
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(i, entry->GetAvatarIconIndex());
#endif
    EXPECT_EQ(true, entry->GetBackgroundStatus());
    EXPECT_EQ(gaia_name, entry->GetGAIAName());
  }
}

TEST_F(ProfileAttributesStorageTest, RemoveProfile) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_EQ(entry, nullptr);

  AddTestingProfile();
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());
  entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(u"testing_profile_name0", entry->GetName());

  // Deleting an existing profile. This should call observers and make the entry
  // un-retrievable.
  AddCallExpectationsForRemoveProfile(0);
  storage()->RemoveProfile(GetProfilePath("testing_profile_path0"));
  VerifyAndResetCallExpectations();
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());
  entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  EXPECT_EQ(entry, nullptr);
}

TEST_F(ProfileAttributesStorageTest, RemoveProfileByAccountId) {
  DisableObserver();  // This test doesn't test observers.
  EXPECT_EQ(0u, storage()->GetNumberOfProfiles());
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

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    ProfileAttributesInitParams params;
    params.profile_path = GetProfilePath(kTestCases[i].profile_path);
    params.profile_name = base::ASCIIToUTF16(kTestCases[i].profile_name);
    params.gaia_id = kTestCases[i].account_id.GetGaiaId();
    params.user_name =
        base::UTF8ToUTF16(kTestCases[i].account_id.GetUserEmail());
    params.is_consented_primary_account =
        kTestCases[i].is_consented_primary_account;
    storage()->AddProfile(std::move(params));
    EXPECT_EQ(i + 1, storage()->GetNumberOfProfiles());
  }

  storage()->RemoveProfileByAccountId(kTestCases[2].account_id);
  EXPECT_EQ(3u, storage()->GetNumberOfProfiles());

  storage()->RemoveProfileByAccountId(kTestCases[0].account_id);
  EXPECT_EQ(2u, storage()->GetNumberOfProfiles());

  // This profile is already deleted.
  storage()->RemoveProfileByAccountId(kTestCases[2].account_id);
  EXPECT_EQ(2u, storage()->GetNumberOfProfiles());

  // Remove profile by partial match.
  storage()->RemoveProfileByAccountId(
      AccountId::FromUserEmail(kTestCases[1].account_id.GetUserEmail()));
  EXPECT_EQ(1u, storage()->GetNumberOfProfiles());

  // Remove last profile.
  storage()->RemoveProfileByAccountId(kTestCases[3].account_id);
  EXPECT_EQ(0u, storage()->GetNumberOfProfiles());
}

TEST_F(ProfileAttributesStorageTest, MultipleProfiles) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  for (size_t i = 0; i < 5; ++i) {
    AddTestingProfile();
    EXPECT_EQ(i + 1, storage()->GetNumberOfProfiles());
    EXPECT_EQ(i + 1, storage()->GetAllProfilesAttributes().size());
    EXPECT_EQ(
        i + 1,
        storage()->GetAllProfilesAttributesSortedByNameWithCheck().size());
    EXPECT_EQ(i + 1,
              storage()->GetAllProfilesAttributesSortedForDisplay().size());
  }

  EXPECT_EQ(5U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(u"testing_profile_name0", entry->GetName());

  AddCallExpectationsForRemoveProfile(0);
  storage()->RemoveProfile(GetProfilePath("testing_profile_path0"));
  VerifyAndResetCallExpectations();
  entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_EQ(entry, nullptr);
  EXPECT_EQ(4U, storage()->GetNumberOfProfiles());

  std::vector<ProfileAttributesEntry*> entries =
      storage()->GetAllProfilesAttributes();
  EXPECT_EQ(4U, entries.size());
  for (auto* attributes_entry : entries) {
    EXPECT_NE(GetProfilePath("testing_profile_path0"),
              attributes_entry->GetPath());
  }

  EXPECT_EQ(4U,
            storage()->GetAllProfilesAttributesSortedByNameWithCheck().size());
  EXPECT_EQ(4U, storage()->GetAllProfilesAttributesSortedForDisplay().size());
}

TEST_F(ProfileAttributesStorageTest, AddStubProfile) {
  DisableObserver();  // This test doesn't test observers.
  EXPECT_EQ(0u, storage()->GetNumberOfProfiles());

  // Add some profiles with and without a '.' in their paths.
  const struct {
    const char* profile_path;
    const char* profile_name;
  } kTestCases[] = {
      {"path.test0", "name_0"},
      {"path_test1", "name_1"},
      {"path.test2", "name_2"},
      {"path_test3", "name_3"},
  };
  const size_t kNumProfiles = std::size(kTestCases);

  for (auto test_case : kTestCases) {
    base::FilePath profile_path = GetProfilePath(test_case.profile_path);
    std::u16string profile_name = base::ASCIIToUTF16(test_case.profile_name);

    ProfileAttributesInitParams params;
    params.profile_path = profile_path;
    params.profile_name = profile_name;
    storage()->AddProfile(std::move(params));
    ProfileAttributesEntry* entry =
        storage()->GetProfileAttributesWithPath(profile_path);
    EXPECT_TRUE(entry);
    EXPECT_EQ(profile_name, entry->GetName());
  }

  ASSERT_EQ(kNumProfiles, storage()->GetNumberOfProfiles());

  // Check that the profiles can be extracted from the local state.
  std::vector<std::string> names;
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& attributes =
      local_state->GetDict(prefs::kProfileAttributes);
  for (const auto kv : attributes) {
    const base::Value& info = kv.second;
    const std::string* name = info.GetDict().FindString("name");
    names.push_back(*name);
  }

  for (size_t i = 0; i < kNumProfiles; i++)
    ASSERT_FALSE(names[i].empty());
}

TEST_F(ProfileAttributesStorageTest, InitialValues) {
#if BUILDFLAG(IS_ANDROID)
  // Android has only one default avatar.
  size_t kIconIndex = 0;
#else
  size_t kIconIndex = 1;
#endif
  base::FilePath profile_path = GetProfilePath("testing_profile_path");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);

  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"testing_profile_name";
  params.gaia_id = "testing_profile_gaia";
  params.user_name = u"testing_profile_username";
  params.is_consented_primary_account = true;
  params.icon_index = kIconIndex;
  params.supervised_user_id = "testing_supervised_user_id";
  params.account_id = AccountId::FromUserEmailGaiaId(
      base::UTF16ToUTF8(params.user_name), params.gaia_id);
  params.is_ephemeral = true;
  params.is_omitted = true;
  params.is_signed_in_with_credential_provider = true;
  storage()->AddProfile(std::move(params));

  VerifyAndResetCallExpectations();

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  VerifyInitialValues(
      entry, profile_path, /*profile_name=*/u"testing_profile_name",
      /*gaia_id=*/"testing_profile_gaia",
      /*user_name=*/u"testing_profile_username",
      /*is_consented_primary_account=*/true, /*icon_index=*/kIconIndex,
      /*supervised_user_id=*/"testing_supervised_user_id",
      /*is_ephemeral=*/true, /*is_omitted=*/true,
      /*is_signed_in_with_credential_provider=*/true);
}

TEST_F(ProfileAttributesStorageTest, InitialValues_Defaults) {
  base::FilePath profile_path = GetProfilePath("testing_profile_path");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);

  ProfileAttributesInitParams params;
  params.profile_path = profile_path;

  // Verify defaults of ProfileAttributesInitParams.
  EXPECT_TRUE(params.profile_name.empty());
  EXPECT_TRUE(params.gaia_id.empty());
  EXPECT_TRUE(params.user_name.empty());
  EXPECT_FALSE(params.is_consented_primary_account);
  EXPECT_EQ(0U, params.icon_index);
  EXPECT_TRUE(params.supervised_user_id.empty());
  EXPECT_TRUE(params.account_id.empty());
  EXPECT_FALSE(params.is_ephemeral);
  EXPECT_FALSE(params.is_omitted);
  EXPECT_FALSE(params.is_signed_in_with_credential_provider);

  storage()->AddProfile(std::move(params));

  VerifyAndResetCallExpectations();

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  VerifyInitialValues(entry, profile_path, /*profile_name=*/std::u16string(),
                      /*gaia_id=*/std::string(), /*user_name=*/std::u16string(),
                      /*is_consented_primary_account=*/false, /*icon_index=*/0,
                      /*supervised_user_id=*/std::string(),
                      /*is_ephemeral=*/false, /*is_omitted=*/false,
                      /*is_signed_in_with_credential_provider=*/false);
}

// Checks that ProfileAttributesStorage doesn't crash when
// ProfileAttributesEntry initialization modifies an attributes entry.
// This is a regression test for https://crbug.com/1180497.
TEST_F(ProfileAttributesStorageTest, ModifyEntryWhileInitializing) {
  DisableObserver();  // This test doesn't test observers.
  base::FilePath profile_path = GetProfilePath("test");
  {
    signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
    AccountId account_id = AccountId::FromUserEmailGaiaId("email", "111111");
    ProfileAttributesInitParams params;
    params.profile_path = profile_path;
    params.profile_name = u"Test";
    params.gaia_id = account_id.GetGaiaId();
    params.user_name = base::UTF8ToUTF16(account_id.GetUserEmail());
    storage()->AddProfile(std::move(params));
    ProfileAttributesEntry* entry =
        storage()->GetProfileAttributesWithPath(profile_path);
    // Set up the state so that ProfileAttributesEntry::Initialize() will modify
    // the entry.
    entry->LockForceSigninProfile(true);
  }
  // Reinitialize ProfileAttributesStorage.
  ResetProfileAttributesStorage();
  storage();  // Should not crash.

  // The IsSigninRequired attribute should be cleaned up.
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  EXPECT_FALSE(entry->IsSigninRequired());
}

TEST_F(ProfileAttributesStorageTest, ProfileNamesOnInit) {
  DisableObserver();  // This test doesn't test observers.
  // Set up the storage with two profiles having the same GAIA given name.
  // The second profile also has a profile name matching the GAIA given name.
  std::u16string kDefaultProfileName = u"Person 1";
  std::u16string kCommonName = u"Joe";

  // Create and initialize the first profile.
  base::FilePath path_1 = GetProfilePath("path_1");
  ProfileAttributesInitParams params_1;
  params_1.profile_path = path_1;
  params_1.profile_name = kDefaultProfileName;
  storage()->AddProfile(std::move(params_1));
  ProfileAttributesEntry* entry_1 =
      storage()->GetProfileAttributesWithPath(path_1);
  entry_1->SetGAIAGivenName(kCommonName);
  EXPECT_EQ(entry_1->GetName(), kCommonName);

  // Create and initialize the second profile.
  base::FilePath path_2 = GetProfilePath("path_2");
  ProfileAttributesInitParams params_2;
  params_2.profile_path = path_2;
  params_2.profile_name = kCommonName;
  storage()->AddProfile(std::move(params_2));
  ProfileAttributesEntry* entry_2 =
      storage()->GetProfileAttributesWithPath(path_2);
  entry_2->SetGAIAGivenName(kCommonName);
  EXPECT_EQ(entry_2->GetName(), kCommonName);

  // The first profile name should be modified.
  EXPECT_EQ(entry_1->GetName(),
            ConcatenateGaiaAndProfileNames(kCommonName, kDefaultProfileName));

  // Reset the storage to test profile names set on initialization.
  ResetProfileAttributesStorage();
  entry_1 = storage()->GetProfileAttributesWithPath(path_1);
  entry_2 = storage()->GetProfileAttributesWithPath(path_2);

  // Freshly initialized entries should not report name changes.
  EXPECT_FALSE(entry_1->HasProfileNameChanged());
  EXPECT_FALSE(entry_2->HasProfileNameChanged());

  EXPECT_EQ(entry_1->GetName(),
            ConcatenateGaiaAndProfileNames(kCommonName, kDefaultProfileName));
  EXPECT_EQ(entry_2->GetName(), kCommonName);
}

TEST_F(ProfileAttributesStorageTest, EntryAccessors) {
  AddTestingProfile();

  base::FilePath path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(path);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(path, entry->GetPath());

  EXPECT_CALL(observer(), OnProfileNameChanged(path, _)).Times(2);
  entry->SetLocalProfileName(u"first_value", true);
  EXPECT_EQ(u"first_value", entry->GetLocalProfileName());
  EXPECT_TRUE(entry->IsUsingDefaultName());
  entry->SetLocalProfileName(u"second_value", false);
  EXPECT_EQ(u"second_value", entry->GetLocalProfileName());
  EXPECT_FALSE(entry->IsUsingDefaultName());
  VerifyAndResetCallExpectations();

  // GaiaIds.
  EXPECT_TRUE(entry->GetGaiaIds().empty());
  base::flat_set<std::string> accounts1({"a"});
  base::flat_set<std::string> accounts2({"b", "c"});
  entry->SetGaiaIds(accounts1);
  EXPECT_EQ(accounts1, entry->GetGaiaIds());
  entry->SetGaiaIds(accounts2);
  EXPECT_EQ(accounts2, entry->GetGaiaIds());
  entry->SetGaiaIds({});
  EXPECT_TRUE(entry->GetGaiaIds().empty());

  TEST_STRING16_ACCESSORS(ProfileAttributesEntry, entry, ShortcutName);
  TEST_ACCESSORS(ProfileAttributesEntry, entry, BackgroundStatus, true, false);

  EXPECT_CALL(observer(), OnProfileNameChanged(path, _)).Times(4);
  TEST_STRING16_ACCESSORS(ProfileAttributesEntry, entry, GAIAName);
  TEST_STRING16_ACCESSORS(ProfileAttributesEntry, entry, GAIAGivenName);
  VerifyAndResetCallExpectations();

  EXPECT_CALL(observer(), OnProfileAvatarChanged(path)).Times(2);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsUsingGAIAPicture);
  VerifyAndResetCallExpectations();

  // IsOmitted() should be set only on ephemeral profiles.
  entry->SetIsEphemeral(true);
  EXPECT_CALL(observer(), OnProfileIsOmittedChanged(path)).Times(2);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsOmitted);
  VerifyAndResetCallExpectations();
  entry->SetIsEphemeral(false);

  EXPECT_CALL(observer(), OnProfileHostedDomainChanged(path)).Times(2);
  TEST_STRING_ACCESSORS(ProfileAttributesEntry, entry, HostedDomain);
  VerifyAndResetCallExpectations();

  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsEphemeral);

  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsUsingDefaultAvatar);
  TEST_STRING_ACCESSORS(ProfileAttributesEntry, entry,
                        LastDownloadedGAIAPictureUrlWithSize);

  EXPECT_CALL(observer(), OnProfileUserManagementAcceptanceChanged(_)).Times(2);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry,
                      UserAcceptedAccountManagement);

  EXPECT_CALL(observer(), OnProfileManagementEnrollmentTokenChanged(path))
      .Times(2);
  TEST_STRING_ACCESSORS(ProfileAttributesEntry, entry,
                        ProfileManagementEnrollmentToken);

  EXPECT_CALL(observer(), OnProfileManagementIdChanged(path)).Times(2);
  TEST_STRING_ACCESSORS(ProfileAttributesEntry, entry, ProfileManagementId);

  VerifyAndResetCallExpectations();
}

TEST_F(ProfileAttributesStorageTest, EntryInternalAccessors) {
  AddTestingProfile();

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(entry, nullptr);

  EXPECT_EQ(GetProfilePath("testing_profile_path0"), entry->GetPath());

  const char key[] = "test";

  // Tests whether the accessors store and retrieve values correctly.
  EXPECT_TRUE(entry->SetString(key, std::string("abcd")));
  ASSERT_TRUE(entry->GetValue(key));
  EXPECT_EQ(base::Value::Type::STRING, entry->GetValue(key)->type());
  EXPECT_EQ(std::string("abcd"), entry->GetString(key));
  EXPECT_EQ(u"abcd", entry->GetString16(key));
  EXPECT_EQ(0.0, entry->GetDouble(key));
  EXPECT_FALSE(entry->GetBool(key));
  EXPECT_FALSE(entry->IsDouble(key));

  EXPECT_TRUE(entry->SetString16(key, u"efgh"));
  ASSERT_TRUE(entry->GetValue(key));
  EXPECT_EQ(base::Value::Type::STRING, entry->GetValue(key)->type());
  EXPECT_EQ(std::string("efgh"), entry->GetString(key));
  EXPECT_EQ(u"efgh", entry->GetString16(key));
  EXPECT_EQ(0.0, entry->GetDouble(key));
  EXPECT_FALSE(entry->GetBool(key));
  EXPECT_FALSE(entry->IsDouble(key));

  EXPECT_TRUE(entry->SetDouble(key, 12.5));
  ASSERT_TRUE(entry->GetValue(key));
  EXPECT_EQ(base::Value::Type::DOUBLE, entry->GetValue(key)->type());
  EXPECT_EQ(std::string(), entry->GetString(key));
  EXPECT_EQ(u"", entry->GetString16(key));
  EXPECT_EQ(12.5, entry->GetDouble(key));
  EXPECT_FALSE(entry->GetBool(key));
  EXPECT_TRUE(entry->IsDouble(key));

  EXPECT_TRUE(entry->SetBool(key, true));
  ASSERT_TRUE(entry->GetValue(key));
  EXPECT_EQ(base::Value::Type::BOOLEAN, entry->GetValue(key)->type());
  EXPECT_EQ(std::string(), entry->GetString(key));
  EXPECT_EQ(u"", entry->GetString16(key));
  EXPECT_EQ(0.0, entry->GetDouble(key));
  EXPECT_TRUE(entry->GetBool(key));
  EXPECT_FALSE(entry->IsDouble(key));

  // Test whether the setters returns correctly. Setters should return true if
  // the previously stored value is different from the new value.
  entry->SetBool(key, true);
  EXPECT_TRUE(entry->SetString(key, std::string("abcd")));
  EXPECT_FALSE(entry->SetString(key, std::string("abcd")));
  EXPECT_FALSE(entry->SetString16(key, u"abcd"));

  EXPECT_TRUE(entry->SetString16(key, u"efgh"));
  EXPECT_FALSE(entry->SetString16(key, u"efgh"));
  EXPECT_FALSE(entry->SetString(key, std::string("efgh")));

  EXPECT_TRUE(entry->SetDouble(key, 12.5));
  EXPECT_FALSE(entry->SetDouble(key, 12.5));
  EXPECT_TRUE(entry->SetDouble(key, 15.0));

  EXPECT_TRUE(entry->SetString(key, std::string("abcd")));

  EXPECT_TRUE(entry->SetBool(key, true));
  EXPECT_FALSE(entry->SetBool(key, true));
  EXPECT_TRUE(entry->SetBool(key, false));

  EXPECT_TRUE(entry->SetString16(key, u"efgh"));

  // If previous data is not there, setters should returns false even if the
  // defaults (empty string, 0.0, or false) are written.
  EXPECT_FALSE(entry->SetString("test1", std::string()));
  EXPECT_FALSE(entry->SetString16("test2", std::u16string()));
  EXPECT_FALSE(entry->SetDouble("test3", 0.0));
  EXPECT_FALSE(entry->SetBool("test4", false));

  // If previous data is in a wrong type, setters should returns false even if
  // the defaults (empty string, 0.0, or false) are written.
  EXPECT_FALSE(entry->SetString("test3", std::string()));
  EXPECT_FALSE(entry->SetString16("test4", std::u16string()));
  EXPECT_FALSE(entry->SetDouble("test1", 0.0));
  EXPECT_FALSE(entry->SetBool("test2", false));
}

TEST_F(ProfileAttributesStorageTest, ProfileActiveTime) {
  AddTestingProfile();

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(entry, nullptr);

  // Check the state before active time is stored.
  const char kActiveTimeKey[] = "active_time";
  EXPECT_FALSE(entry->IsDouble(kActiveTimeKey));
  EXPECT_EQ(base::Time(), entry->GetActiveTime());

  // Store the time and check for the result. Allow for a difference one second
  // because the 64-bit integral representation in base::Time is rounded off to
  // a double, which is what base::Value stores. http://crbug.com/346827
  base::Time lower_bound = base::Time::Now() - base::Seconds(1);
  entry->SetActiveTimeToNow();
  base::Time upper_bound = base::Time::Now() + base::Seconds(1);
  EXPECT_TRUE(entry->IsDouble(kActiveTimeKey));
  EXPECT_LE(lower_bound, entry->GetActiveTime());
  EXPECT_GE(upper_bound, entry->GetActiveTime());

  // If the active time was less than one hour ago, SetActiveTimeToNow should do
  // nothing.
  base::Time past = base::Time::Now() - base::Minutes(10);
  lower_bound = past - base::Seconds(1);
  upper_bound = past + base::Seconds(1);
  ASSERT_TRUE(
      entry->SetDouble(kActiveTimeKey, past.InSecondsFSinceUnixEpoch()));
  base::Time stored_time = entry->GetActiveTime();
  ASSERT_LE(lower_bound, stored_time);
  ASSERT_GE(upper_bound, stored_time);
  entry->SetActiveTimeToNow();
  EXPECT_EQ(stored_time, entry->GetActiveTime());
}

TEST_F(ProfileAttributesStorageTest, AuthInfo) {
  AddTestingProfile();

  base::FilePath path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(path);
  ASSERT_NE(entry, nullptr);

  EXPECT_CALL(observer(), OnProfileAuthInfoChanged(path)).Times(1);
  entry->SetAuthInfo("", std::u16string(), false);
  VerifyAndResetCallExpectations();
  ASSERT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
  EXPECT_EQ(std::u16string(), entry->GetUserName());
  EXPECT_EQ("", entry->GetGAIAId());

  EXPECT_CALL(observer(), OnProfileAuthInfoChanged(path)).Times(1);
  entry->SetAuthInfo("foo", u"bar", true);
  VerifyAndResetCallExpectations();
  ASSERT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(u"bar", entry->GetUserName());
  EXPECT_EQ("foo", entry->GetGAIAId());
}

TEST_F(ProfileAttributesStorageTest, GAIAName) {
  DisableObserver();  // This test doesn't test observers.

  base::FilePath profile_path_1 = GetProfilePath("path_1");
  ProfileAttributesInitParams params_1;
  params_1.profile_path = profile_path_1;
  params_1.profile_name = u"Person 1";
  storage()->AddProfile(std::move(params_1));
  ProfileAttributesEntry* entry_1 =
      storage()->GetProfileAttributesWithPath(profile_path_1);
  base::FilePath profile_path_2 = GetProfilePath("path_2");
  ProfileAttributesInitParams params_2;
  params_2.profile_path = profile_path_2;
  params_2.profile_name = u"Person 2";
  storage()->AddProfile(std::move(params_2));
  ProfileAttributesEntry* entry_2 =
      storage()->GetProfileAttributesWithPath(profile_path_2);

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
      ConcatenateGaiaAndProfileNames(gaia_name, custom_name);
  EXPECT_EQ(expected_profile_name, entry_2->GetName());
  EXPECT_EQ(gaia_name, entry_2->GetGAIAName());
}

TEST_F(ProfileAttributesStorageTest, ConcatenateGaiaNameAndProfileName) {
  DisableObserver();  // This test doesn't test observers.

  // We should only append the profile name to the GAIA name if:
  // - The user has chosen a profile name on purpose.
  // - Two profiles has the sama GAIA name and we need to show it to
  //   clear ambiguity.
  // If one of the two conditions hold, we will show the profile name in this
  // format |GAIA name (Profile local name)|
  // Single profile.
  base::FilePath profile_path_1 = GetProfilePath("path_1");
  ProfileAttributesInitParams params_1;
  params_1.profile_path = profile_path_1;
  params_1.profile_name = u"Person 1";
  storage()->AddProfile(std::move(params_1));
  ProfileAttributesEntry* entry_1 =
      storage()->GetProfileAttributesWithPath(profile_path_1);
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
  base::FilePath profile_path_2 = GetProfilePath("path_2");
  ProfileAttributesInitParams params_2;
  params_2.profile_path = profile_path_2;
  params_2.profile_name = u"Person 2";
  storage()->AddProfile(std::move(params_2));
  ProfileAttributesEntry* entry_2 =
      storage()->GetProfileAttributesWithPath(profile_path_2);
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
  base::FilePath profile_path_3 = GetProfilePath("path_3");
  ProfileAttributesInitParams params_3;
  params_3.profile_path = profile_path_3;
  params_3.profile_name = u"Person 3";
  storage()->AddProfile(std::move(params_3));
  ProfileAttributesEntry* entry_3 =
      storage()->GetProfileAttributesWithPath(profile_path_3);
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

TEST_F(ProfileAttributesStorageTest, SupervisedUsersAccessors) {
  AddTestingProfile();

  base::FilePath path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(entry, nullptr);

  entry->SetSupervisedUserId("");
  ASSERT_FALSE(entry->IsSupervised());

  EXPECT_CALL(observer(), OnProfileSupervisedUserIdChanged(path)).Times(1);
  entry->SetSupervisedUserId("some_supervised_user_id");
  VerifyAndResetCallExpectations();
  ASSERT_TRUE(entry->IsSupervised());

  EXPECT_CALL(observer(), OnProfileSupervisedUserIdChanged(path)).Times(1);
  entry->SetSupervisedUserId(supervised_user::kChildAccountSUID);
  VerifyAndResetCallExpectations();
  ASSERT_TRUE(entry->IsSupervised());
}

TEST_F(ProfileAttributesStorageTest, CreateSupervisedTestingProfile) {
  DisableObserver();  // This test doesn't test observers.

  base::FilePath path_1 =
      testing_profile_manager().CreateTestingProfile("default")->GetPath();
  std::u16string supervised_user_name = u"Supervised User";
  base::FilePath path_2 =
      testing_profile_manager()
          .CreateTestingProfile(
              "test1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
              supervised_user_name, 0, TestingProfile::TestingFactories(),
              /*is_supervised_profile=*/true)
          ->GetPath();
  base::FilePath profile_paths[] = {path_1, path_2};
  for (const base::FilePath& path : profile_paths) {
    ProfileAttributesEntry* entry =
        storage()->GetProfileAttributesWithPath(path);
    bool is_supervised = entry->GetName() == supervised_user_name;
    EXPECT_EQ(is_supervised, entry->IsSupervised());
    std::string supervised_user_id =
        is_supervised ? supervised_user::kChildAccountSUID : "";
    EXPECT_EQ(supervised_user_id, entry->GetSupervisedUserId());
  }
}

TEST_F(ProfileAttributesStorageTest, ReSortTriggered) {
  DisableObserver();  // No need to test observers in this test.

  ProfileAttributesInitParams alpha_params;
  alpha_params.profile_path = GetProfilePath("alpha_path");
  alpha_params.profile_name = u"alpha";
  alpha_params.gaia_id = "alpha_gaia";
  alpha_params.user_name = u"alpha_username";
  alpha_params.is_consented_primary_account = true;
  alpha_params.icon_index = 1;
  storage()->AddProfile(std::move(alpha_params));

  ProfileAttributesInitParams lima_params;
  lima_params.profile_path = GetProfilePath("lime_path");
  lima_params.profile_name = u"lima";
  lima_params.gaia_id = "lima_gaia";
  lima_params.user_name = u"lima_username";
  lima_params.is_consented_primary_account = true;
  lima_params.icon_index = 1;
  storage()->AddProfile(std::move(lima_params));

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(GetProfilePath("alpha_path"));
  ASSERT_NE(entry, nullptr);

  // Trigger a ProfileAttributesStorage re-sort.
  entry->SetLocalProfileName(u"zulu_name",
                             /*is_default_name=*/false);
  EXPECT_EQ(GetProfilePath("alpha_path"), entry->GetPath());
}

TEST_F(ProfileAttributesStorageTest, RemoveOtherProfile) {
  AddTestingProfile();
  AddTestingProfile();

  EXPECT_EQ(2U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* first_entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(first_entry, nullptr);

  ProfileAttributesEntry* second_entry =
      storage()->GetProfileAttributesWithPath(
          GetProfilePath("testing_profile_path1"));
  ASSERT_NE(second_entry, nullptr);

  EXPECT_EQ(u"testing_profile_name0", first_entry->GetName());

  AddCallExpectationsForRemoveProfile(1);
  storage()->RemoveProfile(GetProfilePath("testing_profile_path1"));
  VerifyAndResetCallExpectations();
  second_entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path1"));
  ASSERT_EQ(second_entry, nullptr);

  EXPECT_EQ(GetProfilePath("testing_profile_path0"), first_entry->GetPath());
  EXPECT_EQ(u"testing_profile_name0", first_entry->GetName());
}

TEST_F(ProfileAttributesStorageTest, AccessFromElsewhere) {
  AddTestingProfile();

  DisableObserver();  // No need to test observers in this test.

  ProfileAttributesEntry* first_entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(first_entry, nullptr);

  ProfileAttributesEntry* second_entry =
      storage()->GetProfileAttributesWithPath(
          GetProfilePath("testing_profile_path0"));
  ASSERT_NE(second_entry, nullptr);

  first_entry->SetLocalProfileName(u"NewName",
                                   /*is_default_name=*/false);
  EXPECT_EQ(u"NewName", second_entry->GetName());
  EXPECT_EQ(first_entry, second_entry);

  second_entry->SetLocalProfileName(u"OtherNewName",
                                    /*is_default_name=*/false);
  EXPECT_EQ(u"OtherNewName", first_entry->GetName());
}

TEST_F(ProfileAttributesStorageTest, ChooseAvatarIconIndexForNewProfile) {
  size_t total_icon_count = profiles::GetDefaultAvatarIconCount() -
                            profiles::GetModernAvatarIconStartIndex();

  // Run ChooseAvatarIconIndexForNewProfile |num_iterations| times before using
  // the final |icon_index| to add a profile. Multiple checks are needed because
  // ChooseAvatarIconIndexForNewProfile is non-deterministic.
  const int num_iterations = 10;
  std::unordered_set<int> used_icon_indices;

  for (size_t i = 0; i < total_icon_count; ++i) {
    EXPECT_EQ(i, storage()->GetNumberOfProfiles());

    size_t icon_index = 0;
    for (int iter = 0; iter < num_iterations; ++iter) {
      icon_index = storage()->ChooseAvatarIconIndexForNewProfile();
      // Icon must not be used.
      ASSERT_EQ(0u, used_icon_indices.count(icon_index));
      ASSERT_TRUE(profiles::IsModernAvatarIconIndex(icon_index));
    }

    used_icon_indices.insert(icon_index);

    base::FilePath profile_path =
        GetProfilePath(base::StringPrintf("testing_profile_path%" PRIuS, i));
    EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
    ProfileAttributesInitParams params;
    params.profile_path = profile_path;
    params.icon_index = icon_index;
    storage()->AddProfile(std::move(params));
    VerifyAndResetCallExpectations();
  }

  for (int iter = 0; iter < num_iterations; ++iter) {
    // All icons are used up, expect any valid icon.
    ASSERT_TRUE(profiles::IsModernAvatarIconIndex(
        storage()->ChooseAvatarIconIndexForNewProfile()));
  }
}

TEST_F(ProfileAttributesStorageTest, IsSigninRequiredOnInit_NotAuthenticated) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);

  base::FilePath profile_path = GetProfilePath("testing_profile_path");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);

  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"testing_profile_name";
  params.is_consented_primary_account = false;
  storage()->AddProfile(std::move(params));

  VerifyAndResetCallExpectations();

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_TRUE(entry->IsSigninRequired());
}

TEST_F(ProfileAttributesStorageTest, IsSigninRequiredOnInit_Authenticated) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);

  base::FilePath profile_path = GetProfilePath("testing_profile_path");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);

  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"testing_profile_name";
  params.gaia_id = "testing_profile_gaia";
  params.user_name = u"testing_profile_username";
  params.is_consented_primary_account = true;
  storage()->AddProfile(std::move(params));

  VerifyAndResetCallExpectations();

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsSigninRequired());
}

TEST_F(ProfileAttributesStorageTest,
       IsSigninRequiredOnInit_FromPreviousSession) {
  base::FilePath profile_path = GetProfilePath("testing_profile_path");
  {
    signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);

    EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);

    ProfileAttributesInitParams params;
    params.profile_path = profile_path;
    params.profile_name = u"testing_profile_name";
    params.gaia_id = "testing_profile_gaia";
    params.user_name = u"testing_profile_username";
    params.is_consented_primary_account = true;
    storage()->AddProfile(std::move(params));

    VerifyAndResetCallExpectations();

    // IsSigninRequired() cannot be set as an init parameter. Set it after an
    // entry is initialized and reset the storage to reinitialize an entry from
    // prefs.
    EXPECT_CALL(observer(), OnProfileSigninRequiredChanged(profile_path))
        .Times(1);
    ProfileAttributesEntry* entry =
        storage()->GetProfileAttributesWithPath(profile_path);
    entry->LockForceSigninProfile(true);
    VerifyAndResetCallExpectations();
    ResetProfileAttributesStorage();

    entry = storage()->GetProfileAttributesWithPath(profile_path);
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->IsAuthenticated());
    EXPECT_TRUE(entry->IsSigninRequired());
  }

  // Reset the storage once more after the policy has been disabled and check
  // that sign-in is no longer required.
  ResetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsSigninRequired());
}

TEST_F(ProfileAttributesStorageTest, ProfileForceSigninLock) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);

  AddTestingProfile();

  base::FilePath path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(path);
  ASSERT_NE(entry, nullptr);
  ASSERT_FALSE(entry->IsSigninRequired());

  entry->LockForceSigninProfile(false);
  ASSERT_FALSE(entry->IsSigninRequired());

  EXPECT_CALL(observer(), OnProfileSigninRequiredChanged(path)).Times(1);
  entry->LockForceSigninProfile(true);
  VerifyAndResetCallExpectations();
  ASSERT_TRUE(entry->IsSigninRequired());

  EXPECT_CALL(observer(), OnProfileSigninRequiredChanged(path)).Times(1);
  entry->LockForceSigninProfile(false);
  VerifyAndResetCallExpectations();
  ASSERT_FALSE(entry->IsSigninRequired());
}

// Avatar icons not used on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ProfileAttributesStorageTest, AvatarIconIndex) {
  AddTestingProfile();

  base::FilePath profile_path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(0U, entry->GetAvatarIconIndex());

  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetAvatarIconIndex(2U);
  VerifyAndResetCallExpectations();
  ASSERT_EQ(2U, entry->GetAvatarIconIndex());

  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetAvatarIconIndex(3U);
  VerifyAndResetCallExpectations();
  ASSERT_EQ(3U, entry->GetAvatarIconIndex());
}
#endif

// High res avatar downloading is only supported on desktop.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileAttributesStorageTest, DownloadHighResAvatarTest) {
  storage()->set_disable_avatar_download_for_testing(false);

  const size_t kIconIndex = 0;
  base::FilePath icon_path =
      profiles::GetPathOfHighResAvatarAtIndex(kIconIndex);

  ASSERT_EQ(0U, storage()->GetNumberOfProfiles());
  base::FilePath profile_path = GetProfilePath("path_1");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"name_1";
  params.icon_index = kIconIndex;
  storage()->AddProfile(std::move(params));
  ASSERT_EQ(1U, storage()->GetNumberOfProfiles());
  VerifyAndResetCallExpectations();

  // Make sure there are no avatars already on disk.
  content::RunAllTasksUntilIdle();
  ASSERT_FALSE(base::PathExists(icon_path));

  // We haven't downloaded any high-res avatars yet.
  EXPECT_EQ(0U, storage()->cached_avatar_images_.size());

  // After adding a new profile, the download of high-res avatar will be
  // triggered. But the downloader won't ever call OnFetchComplete in the test.
  EXPECT_EQ(1U, storage()->avatar_images_downloads_in_progress_.size());

  // |GetHighResAvater| does not contain a cached avatar, so it should return
  // null.
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->GetHighResAvatar());

  // The previous |GetHighResAvater| starts |LoadAvatarPictureFromPath| async.
  // The async code will end up at |OnAvatarPictureLoaded| storing an empty
  // image in the storage.
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  content::RunAllTasksUntilIdle();
  VerifyAndResetCallExpectations();
  std::string icon_filename =
      profiles::GetDefaultAvatarIconFileNameAtIndex(kIconIndex);
  EXPECT_EQ(1U, storage()->cached_avatar_images_.size());
  EXPECT_TRUE(storage()->cached_avatar_images_[icon_filename].IsEmpty());

  // Simulate downloading a high-res avatar.
  ProfileAvatarDownloader avatar_downloader(
      kIconIndex,
      base::BindOnce(&ProfileAttributesStorage::SaveAvatarImageAtPathNoCallback,
                     base::Unretained(storage()), entry->GetPath()));

  // Put a real bitmap into "bitmap": a 2x2 bitmap of green 32 bit pixels.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  bitmap.eraseColor(SK_ColorGREEN);

  avatar_downloader.OnFetchComplete(GURL("http://www.google.com/avatar.png"),
                                    &bitmap);

  // Now the download should not be in progress anymore.
  EXPECT_EQ(0U, storage()->avatar_images_downloads_in_progress_.size());

  // The image should have been cached.
  EXPECT_EQ(1U, storage()->cached_avatar_images_.size());
  EXPECT_FALSE(storage()->cached_avatar_images_[icon_filename].IsEmpty());
  EXPECT_EQ(&storage()->cached_avatar_images_[icon_filename],
            entry->GetHighResAvatar());

  // Since we are not using GAIA image, |GetAvatarIcon| should return the same
  // image as |GetHighResAvatar| in desktop. Since it returns a copy, the
  // backing object needs to get checked.
  const gfx::ImageSkia* avatar_icon = entry->GetAvatarIcon().ToImageSkia();
  const gfx::ImageSkia* cached_icon =
      storage()->cached_avatar_images_[icon_filename].ToImageSkia();
  EXPECT_TRUE(avatar_icon->BackedBySameObjectAs(*cached_icon));

  // Finish the async calls that save the image to the disk.
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  content::RunAllTasksUntilIdle();
  VerifyAndResetCallExpectations();

  // Clean up.
  EXPECT_NE(std::string::npos, icon_path.MaybeAsASCII().find(icon_filename));
  ASSERT_TRUE(base::PathExists(icon_path));
  EXPECT_TRUE(base::DeleteFile(icon_path));
  EXPECT_FALSE(base::PathExists(icon_path));
}

TEST_F(ProfileAttributesStorageTest, NothingToDownloadHighResAvatarTest) {
  storage()->set_disable_avatar_download_for_testing(false);

  const size_t kIconIndex = profiles::GetPlaceholderAvatarIndex();

  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());
  base::FilePath profile_path = GetProfilePath("path_1");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"name_1";
  params.icon_index = kIconIndex;
  storage()->AddProfile(std::move(params));
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());
  content::RunAllTasksUntilIdle();

  // We haven't tried to download any high-res avatars as the specified icon is
  // just a placeholder.
  EXPECT_EQ(0U, storage()->cached_avatar_images_.size());
  EXPECT_EQ(0U, storage()->avatar_images_downloads_in_progress_.size());
}

TEST_F(ProfileAttributesStorageTest, LoadAvatarFromDiskTest) {
  const size_t kIconIndex = 0;
  base::FilePath icon_path =
      profiles::GetPathOfHighResAvatarAtIndex(kIconIndex);

  // Create the avatar on the disk, which is a valid 1x1 transparent png.
  base::FilePath dir = icon_path.DirName();
  ASSERT_FALSE(base::DirectoryExists(dir));
  ASSERT_TRUE(base::CreateDirectory(dir));
  ASSERT_FALSE(base::PathExists(icon_path));
  const char bitmap[] =
      "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A\x00\x00\x00\x0D\x49\x48\x44\x52"
      "\x00\x00\x00\x01\x00\x00\x00\x01\x01\x00\x00\x00\x00\x37\x6E\xF9"
      "\x24\x00\x00\x00\x0A\x49\x44\x41\x54\x08\x1D\x63\x60\x00\x00\x00"
      "\x02\x00\x01\xCF\xC8\x35\xE5\x00\x00\x00\x00\x49\x45\x4E\x44\xAE"
      "\x42\x60\x82";
  base::WriteFile(icon_path, std::string_view(bitmap, sizeof(bitmap)));
  ASSERT_TRUE(base::PathExists(icon_path));

  // Add a new profile.
  ASSERT_EQ(0U, storage()->GetNumberOfProfiles());
  base::FilePath profile_path = GetProfilePath("path_1");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"name_1";
  params.icon_index = kIconIndex;
  storage()->AddProfile(std::move(params));
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());
  VerifyAndResetCallExpectations();

  // Load the avatar image.
  storage()->set_disable_avatar_download_for_testing(false);
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  ASSERT_FALSE(entry->IsUsingGAIAPicture());
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  entry->GetAvatarIcon();

  // Wait until the avatar image finish loading.
  content::RunAllTasksUntilIdle();
  VerifyAndResetCallExpectations();

  // Clean up.
  EXPECT_TRUE(base::DeleteFile(icon_path));
  EXPECT_FALSE(base::PathExists(icon_path));
}
#endif

TEST_F(ProfileAttributesStorageTest, ProfilesState_ActiveMultiProfile) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());
  for (size_t i = 0; i < 5; ++i)
    AddTestingProfile();
  EXPECT_EQ(5U, storage()->GetNumberOfProfiles());

  std::vector<ProfileAttributesEntry*> entries =
      storage()->GetAllProfilesAttributes();
  entries[0]->SetActiveTimeToNow();
  entries[1]->SetActiveTimeToNow();

  base::HistogramTester histogram_tester;
  storage()->RecordProfilesState();

  // There are 5 profiles all together.
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_All", 5);
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_ActiveMultiProfile",
                                    5);

  // Other user segments get 0 records.
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_SingleProfile", 0);
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_LatentMultiProfile",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Profile.State.LastUsed_LatentMultiProfileActive", 0);
  histogram_tester.ExpectTotalCount(
      "Profile.State.LastUsed_LatentMultiProfileOthers", 0);
}

// On Android (at least on KitKat), all profiles are considered active (because
// ActiveTime is not set in production). Thus, these test does not work.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ProfileAttributesStorageTest, ProfilesState_LatentMultiProfile) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());
  for (size_t i = 0; i < 5; ++i)
    AddTestingProfile();
  EXPECT_EQ(5U, storage()->GetNumberOfProfiles());

  std::vector<ProfileAttributesEntry*> entries =
      storage()->GetAllProfilesAttributes();
  entries[0]->SetActiveTimeToNow();

  base::HistogramTester histogram_tester;
  storage()->RecordProfilesState();

  // There are 5 profiles all together.
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_All", 5);
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_LatentMultiProfile",
                                    5);
  histogram_tester.ExpectTotalCount(
      "Profile.State.LastUsed_LatentMultiProfileActive", 1);
  histogram_tester.ExpectTotalCount(
      "Profile.State.LastUsed_LatentMultiProfileOthers", 4);

  // Other user segments get 0 records.
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_SingleProfile", 0);
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_ActiveMultiProfile",
                                    0);
}
#endif

TEST_F(ProfileAttributesStorageTest, ProfilesState_SingleProfile) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());
  AddTestingProfile();
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());

  base::HistogramTester histogram_tester;
  storage()->RecordProfilesState();

  // There is 1 profile all together.
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_All", 1);
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_SingleProfile", 1);

  // Other user segments get 0 records.
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_ActiveMultiProfile",
                                    0);
  histogram_tester.ExpectTotalCount("Profile.State.LastUsed_LatentMultiProfile",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Profile.State.LastUsed_LatentMultiProfileActive", 0);
  histogram_tester.ExpectTotalCount(
      "Profile.State.LastUsed_LatentMultiProfileOthers", 0);
}

// Themes aren't used on Android
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ProfileAttributesStorageTest, ProfileThemeColors) {
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(false);
  AddTestingProfile();
  base::FilePath profile_path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetAvatarIconIndex(profiles::GetPlaceholderAvatarIndex());
  VerifyAndResetCallExpectations();

  ProfileThemeColors light_colors = GetDefaultProfileThemeColors();
  EXPECT_EQ(entry->GetProfileThemeColors(), light_colors);

  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_use_dark_colors(true);
  EXPECT_EQ(entry->GetProfileThemeColors(), GetDefaultProfileThemeColors());
  EXPECT_NE(entry->GetProfileThemeColors(), light_colors);

  ProfileThemeColors colors = {SK_ColorTRANSPARENT, SK_ColorBLACK,
                               SK_ColorWHITE};
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileThemeColorsChanged(profile_path)).Times(1);
  entry->SetProfileThemeColors(colors);
  EXPECT_EQ(entry->GetProfileThemeColors(), colors);
  VerifyAndResetCallExpectations();

  // Colors shouldn't change after switching back to the light mode.
  native_theme->set_use_dark_colors(false);
  EXPECT_EQ(entry->GetProfileThemeColors(), colors);

  // std::nullopt resets the colors to default.
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileThemeColorsChanged(profile_path)).Times(1);
  entry->SetProfileThemeColors(std::nullopt);
  EXPECT_EQ(entry->GetProfileThemeColors(), GetDefaultProfileThemeColors());
  VerifyAndResetCallExpectations();
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ProfileAttributesStorageTest, GAIAPicture) {
  const int kDefaultAvatarIndex = 0;
  const int kOtherAvatarIndex = 1;
  const int kGaiaPictureSize = 256;  // Standard size of a Gaia account picture.
  base::FilePath profile_path = GetProfilePath("path_1");
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"name_1";
  params.icon_index = kDefaultAvatarIndex;
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  storage()->AddProfile(std::move(params));
  VerifyAndResetCallExpectations();
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);

  // Sanity check.
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_FALSE(entry->IsUsingGAIAPicture());

  // The profile icon should be the default one.
  EXPECT_TRUE(entry->IsUsingDefaultAvatar());
  size_t default_avatar_id =
      GetDefaultAvatarIconResourceIDAtIndex(kDefaultAvatarIndex);
  const gfx::Image& default_avatar_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(default_avatar_id));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(default_avatar_image, entry->GetAvatarIcon()));

  // Set GAIA picture.
  gfx::Image gaia_image(
      gfx::test::CreateImage(kGaiaPictureSize, kGaiaPictureSize));
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetGAIAPicture("GAIA_IMAGE_URL_WITH_SIZE_1", gaia_image);
  VerifyAndResetCallExpectations();
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, *entry->GetGAIAPicture()));
  // Since we're still using the default avatar, the GAIA image should be
  // preferred over the generic avatar image.
  EXPECT_TRUE(entry->IsUsingDefaultAvatar());
  EXPECT_TRUE(entry->IsUsingGAIAPicture());
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, entry->GetAvatarIcon()));

  // Set a non-default avatar. This should be preferred over the GAIA image.
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetAvatarIconIndex(kOtherAvatarIndex);
  entry->SetIsUsingDefaultAvatar(false);
  VerifyAndResetCallExpectations();
  EXPECT_FALSE(entry->IsUsingDefaultAvatar());
  EXPECT_FALSE(entry->IsUsingGAIAPicture());
// Avatar icons not used on Android.
#if !BUILDFLAG(IS_ANDROID)

  size_t other_avatar_id =
      GetDefaultAvatarIconResourceIDAtIndex(kOtherAvatarIndex);
  const gfx::Image& other_avatar_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(other_avatar_id));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(other_avatar_image, entry->GetAvatarIcon()));
#endif  // !BUILDFLAG(IS_ANDROID)

  // Explicitly setting the GAIA picture should make it preferred again.
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetIsUsingGAIAPicture(true);
  VerifyAndResetCallExpectations();
  EXPECT_TRUE(entry->IsUsingGAIAPicture());
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, *entry->GetGAIAPicture()));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, entry->GetAvatarIcon()));

  // Clearing the IsUsingGAIAPicture flag should result in the generic image
  // being used again.
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetIsUsingGAIAPicture(false);
  VerifyAndResetCallExpectations();
  EXPECT_FALSE(entry->IsUsingGAIAPicture());
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, *entry->GetGAIAPicture()));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(other_avatar_image, entry->GetAvatarIcon()));
#endif
}

TEST_F(ProfileAttributesStorageTest, PersistGAIAPicture) {
  base::FilePath profile_path = GetProfilePath("path_1");
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"name_1";
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  storage()->AddProfile(std::move(params));
  VerifyAndResetCallExpectations();
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  gfx::Image gaia_image(gfx::test::CreateImage(100, 50));

  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  entry->SetGAIAPicture("GAIA_IMAGE_URL_WITH_SIZE_0", gaia_image);
  // Make sure everything has completed, and the file has been written to disk.
  content::RunAllTasksUntilIdle();
  VerifyAndResetCallExpectations();

  EXPECT_EQ(entry->GetLastDownloadedGAIAPictureUrlWithSize(),
            "GAIA_IMAGE_URL_WITH_SIZE_0");
  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, *entry->GetGAIAPicture()));

  ResetProfileAttributesStorage();
  // Try to get the GAIA picture. This should return NULL until the read from
  // disk is done.
  entry = storage()->GetProfileAttributesWithPath(profile_path);
  EXPECT_EQ(nullptr, entry->GetGAIAPicture());
  EXPECT_EQ(entry->GetLastDownloadedGAIAPictureUrlWithSize(),
            "GAIA_IMAGE_URL_WITH_SIZE_0");
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(gfx::test::AreImagesEqual(gaia_image, *entry->GetGAIAPicture()));
}

TEST_F(ProfileAttributesStorageTest, EmptyGAIAInfo) {
  std::u16string profile_name = u"name_1";
  size_t id = GetDefaultAvatarIconResourceIDAtIndex(0);
  const gfx::Image& profile_image(
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(id));

  base::FilePath profile_path = GetProfilePath("path_1");
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = profile_name;
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  storage()->AddProfile(std::move(params));
  VerifyAndResetCallExpectations();
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);

  gfx::Image gaia_image(gfx::test::CreateImage(100, 50));
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  entry->SetGAIAPicture("GAIA_IMAGE_URL_WITH_SIZE_0", gaia_image);
  // Make sure everything has completed, and the file has been written to disk.
  content::RunAllTasksUntilIdle();
  VerifyAndResetCallExpectations();

  // Set empty GAIA info.
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(2);
  entry->SetGAIAName(std::u16string());
  entry->SetGAIAPicture(std::string(), gfx::Image());
  entry->SetIsUsingGAIAPicture(true);

  EXPECT_TRUE(entry->GetLastDownloadedGAIAPictureUrlWithSize().empty());

  // Verify that the profile name and picture are not empty.
  EXPECT_EQ(profile_name, entry->GetName());
  EXPECT_TRUE(gfx::test::AreImagesEqual(profile_image, entry->GetAvatarIcon()));
}

TEST_F(ProfileAttributesStorageTest, GetAllProfilesKeys) {
  PrefService* local_state = g_browser_process->local_state();

  // Check there are initially no profiles.
  EXPECT_EQ(ProfileAttributesStorage::GetAllProfilesKeys(local_state),
            base::flat_set<std::string>());

  // Add a profile, and check that it is returned.
  AddTestingProfile();
  EXPECT_EQ(ProfileAttributesStorage::GetAllProfilesKeys(local_state),
            base::flat_set<std::string>({base::StringPrintf(
                "testing_profile_path%" PRIuS, (size_t)0U)}));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileAttributesStorageTest, GetGaiaImageForAvatarMenu) {
  storage()->set_disable_avatar_download_for_testing(false);

  base::FilePath profile_path = GetProfilePath("path_1");
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"name_1";
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  storage()->AddProfile(std::move(params));
  VerifyAndResetCallExpectations();
  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);

  gfx::Image gaia_image(gfx::test::CreateImage(100, 50));
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  entry->SetGAIAPicture("GAIA_IMAGE_URL_WITH_SIZE_0", gaia_image);
  // Make sure everything has completed, and the file has been written to disk.
  content::RunAllTasksUntilIdle();
  VerifyAndResetCallExpectations();
  // Make sure this profile is using GAIA picture.
  EXPECT_TRUE(entry->IsUsingGAIAPicture());

  ResetProfileAttributesStorage();
  entry = storage()->GetProfileAttributesWithPath(profile_path);

  // We need to explicitly set the GAIA usage flag after resetting the storage.
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileHighResAvatarLoaded(profile_path)).Times(1);
  entry->SetIsUsingGAIAPicture(true);
  EXPECT_TRUE(entry->IsUsingGAIAPicture());

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
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileAttributesStorageTest,
       MigrateLegacyProfileNamesAndRecomputeIfNeeded) {
  DisableObserver();  // This test doesn't test observers.
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());
  // Mimic a pre-existing Directory with profiles that has legacy profile
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
  const size_t kNumProfiles = std::size(kTestCases);

  ProfileAttributesEntry* entry = nullptr;
  for (size_t i = 0; i < kNumProfiles; ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    ProfileAttributesInitParams params;
    params.profile_path = profile_path;
    params.profile_name = base::ASCIIToUTF16(kTestCases[i].profile_name);
    params.icon_index = i;
    storage()->AddProfile(std::move(params));
    entry = storage()->GetProfileAttributesWithPath(profile_path);
    EXPECT_TRUE(entry);
    entry->SetLocalProfileName(entry->GetLocalProfileName(),
                               kTestCases[i].is_using_default_name);
  }

  EXPECT_EQ(kNumProfiles, storage()->GetNumberOfProfiles());

  ResetProfileAttributesStorage();
  ProfileAttributesStorage::SetLegacyProfileMigrationForTesting(true);
  storage();
  ProfileAttributesStorage::SetLegacyProfileMigrationForTesting(false);

  entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath(kTestCases[4].profile_path));
  EXPECT_EQ(base::ASCIIToUTF16(kTestCases[4].profile_name), entry->GetName());
  entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath(kTestCases[10].profile_path));
  EXPECT_EQ(base::ASCIIToUTF16(kTestCases[10].profile_name), entry->GetName());

  // Legacy profile names like "Default Profile" and "First user" should be
  // migrated to "Person %n" type names, i.e. any permutation of "Person %n".
  std::set<std::u16string> expected_profile_names{
      u"Person 1", u"Person 2", u"Person 3", u"Person 4", u"Person 5",
      u"Person 6", u"Person 7", u"Person 8", u"Person 9", u"Person 10"};

  const char* profile_paths[] = {
      kTestCases[0].profile_path, kTestCases[1].profile_path,
      kTestCases[2].profile_path, kTestCases[3].profile_path,
      kTestCases[5].profile_path, kTestCases[6].profile_path,
      kTestCases[7].profile_path, kTestCases[8].profile_path,
      kTestCases[9].profile_path, kTestCases[11].profile_path};

  std::set<std::u16string> actual_profile_names;
  for (auto* path : profile_paths) {
    entry = storage()->GetProfileAttributesWithPath(GetProfilePath(path));
    actual_profile_names.insert(entry->GetName());
  }
  EXPECT_EQ(actual_profile_names, expected_profile_names);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ProfileAttributesStorageTest,
       InitialSavedOrderValidWithAddRemoveProfiles) {
  DisableObserver();

  ASSERT_EQ(0U, storage()->GetNumberOfProfiles());
  ASSERT_EQ(0U, storage()->GetAllProfilesAttributesSortedForDisplay().size());

  const std::u16string profile1(u"D");
  const std::u16string profile2(u"B");
  const std::u16string profile3(u"C");

  // Add two initial profiles.
  AddSimpleTestingProfileWithName(profile1);
  AddSimpleTestingProfileWithName(profile2);
  ASSERT_EQ(2U, storage()->GetNumberOfProfiles());

  // Check the initial saved order is the same as the profile insertion order
  // and not based on the Profile Name.
  {
    auto saved_order_entries =
        storage()->GetAllProfilesAttributesSortedForDisplay();
    ASSERT_EQ(2U, saved_order_entries.size());
    EXPECT_EQ(profile1, saved_order_entries[0]->GetLocalProfileName());
    EXPECT_EQ(profile2, saved_order_entries[1]->GetLocalProfileName());
  }

  // Add a third profile.
  AddSimpleTestingProfileWithName(profile3);
  ASSERT_EQ(3U, storage()->GetNumberOfProfiles());

  // Check after one more insertion.
  {
    auto saved_order_entries =
        storage()->GetAllProfilesAttributesSortedForDisplay();
    ASSERT_EQ(3U, saved_order_entries.size());
    EXPECT_EQ(profile1, saved_order_entries[0]->GetLocalProfileName());
    EXPECT_EQ(profile2, saved_order_entries[1]->GetLocalProfileName());
    EXPECT_EQ(profile3, saved_order_entries[2]->GetLocalProfileName());
  }

  // Remove the second profile that was added.
  storage()->RemoveProfile(GetProfilePath(base::UTF16ToASCII(profile2)));
  ASSERT_EQ(2U, storage()->GetNumberOfProfiles());

  // Check after removing the second profile profile.
  {
    auto saved_order_entries =
        storage()->GetAllProfilesAttributesSortedForDisplay();
    ASSERT_EQ(2U, saved_order_entries.size());
    EXPECT_EQ(profile1, saved_order_entries[0]->GetLocalProfileName());
    EXPECT_EQ(profile3, saved_order_entries[1]->GetLocalProfileName());
  }
}

TEST_F(ProfileAttributesStorageTest, RecoverProfileOrderPrefAfterIssues) {
  DisableObserver();

  base::Value::List profile_keys;
  profile_keys.with_capacity(3);
  profile_keys.Append(u"D");
  profile_keys.Append(u"B");
  profile_keys.Append(u"C");

  for (auto& profile_key : profile_keys) {
    AddSimpleTestingProfileWithName(
        base::ASCIIToUTF16(profile_key.GetString()));
  }

  PrefService* local_state = g_browser_process->local_state();
  ScopedListPrefUpdate update(local_state, prefs::kProfilesOrder);
  base::Value::List& profiles_order = update.Get();

  ASSERT_EQ(profile_keys, profiles_order);

  // After recovery, the expected order is modified to be alphabetically
  // ordered.
  base::Value::List expected_recovered_keys;
  expected_recovered_keys.with_capacity(profile_keys.size());
  expected_recovered_keys.Append(profile_keys[1].GetString());
  expected_recovered_keys.Append(profile_keys[2].GetString());
  expected_recovered_keys.Append(profile_keys[0].GetString());

  {
    // Simulate an issue with a lost profile in the pref.
    profiles_order.EraseValue(profile_keys[0]);
    ASSERT_NE(profiles_order.size(), expected_recovered_keys.size());
    storage()->EnsureProfilesOrderPrefIsInitializedForTesting();
    EXPECT_EQ(profiles_order, expected_recovered_keys);
  }

  {
    // Simulate an issue where a key is duplicated.
    profiles_order[0] = base::Value(profiles_order[1].GetString());
    ASSERT_EQ(profiles_order[0], profiles_order[1]);
    ASSERT_NE(profiles_order[0], expected_recovered_keys[0]);
    ASSERT_NE(expected_recovered_keys[0], expected_recovered_keys[1]);
    storage()->EnsureProfilesOrderPrefIsInitializedForTesting();
    EXPECT_EQ(profiles_order, expected_recovered_keys);
  }

  {
    // Simulate an issue where a key does not match an entry.
    profiles_order[0] = base::Value(u"DBC");
    ASSERT_NE(profiles_order[0], expected_recovered_keys[0]);
    storage()->EnsureProfilesOrderPrefIsInitializedForTesting();
    EXPECT_EQ(profiles_order, expected_recovered_keys);
  }
}

TEST_F(ProfileAttributesStorageTest, UpdateProfilesOrderPref) {
  DisableObserver();

  AddSimpleTestingProfileWithName(u"A");
  AddSimpleTestingProfileWithName(u"B");
  AddSimpleTestingProfileWithName(u"C");
  AddSimpleTestingProfileWithName(u"D");

  base::HistogramTester histogram_tester;

  {
    std::vector<std::string> expected_keys{"A", "B", "C", "D"};
    ASSERT_EQ(
        EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
        expected_keys);
    histogram_tester.ExpectUniqueSample("Profile.ProfilesOrderChanged", true,
                                        0u);
  }

  {
    storage()->UpdateProfilesOrderPref(0, 1);
    std::vector<std::string> expected_keys{"B", "A", "C", "D"};
    EXPECT_EQ(
        EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
        expected_keys);
    histogram_tester.ExpectUniqueSample("Profile.ProfilesOrderChanged", true,
                                        1u);
  }

  {
    storage()->UpdateProfilesOrderPref(3, 0);
    std::vector<std::string> expected_keys{"D", "B", "A", "C"};
    EXPECT_EQ(
        EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
        expected_keys);
    histogram_tester.ExpectUniqueSample("Profile.ProfilesOrderChanged", true,
                                        2u);
  }
}

// This test makes sure that performing the inverse of an action will result in
// the same initial result. Makes sure that there is a way to come back to the
// original state.
TEST_F(ProfileAttributesStorageTest, UpdateProfilesOrderPrefIsSymetric) {
  DisableObserver();

  AddSimpleTestingProfileWithName(u"A");
  AddSimpleTestingProfileWithName(u"B");
  AddSimpleTestingProfileWithName(u"C");
  AddSimpleTestingProfileWithName(u"D");

  base::HistogramTester histogram_tester;

  std::vector<std::string> initial_keys_order{"A", "B", "C", "D"};
  ASSERT_EQ(
      EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
      initial_keys_order);

  int from_index = 1;
  int to_index = 3;
  {
    // Initial shift.
    storage()->UpdateProfilesOrderPref(from_index, to_index);
    std::vector<std::string> expected_keys{"A", "C", "D", "B"};
    EXPECT_EQ(
        EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
        expected_keys);
  }

  // Perform the reverse of the initial shift by inverting the inputs.
  storage()->UpdateProfilesOrderPref(to_index, from_index);
  EXPECT_EQ(
      EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
      initial_keys_order);

  histogram_tester.ExpectUniqueSample("Profile.ProfilesOrderChanged", true, 2u);
}

TEST_F(ProfileAttributesStorageTest, UpdateProfilesOrderPrefSameIndex) {
  DisableObserver();

  AddSimpleTestingProfileWithName(u"A");
  AddSimpleTestingProfileWithName(u"B");
  AddSimpleTestingProfileWithName(u"C");

  base::HistogramTester histogram_tester;

  std::vector<std::string> initial_keys_order{"A", "B", "C"};
  ASSERT_EQ(
      EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
      initial_keys_order);

  int index = 2;
  // Use the same index as from and to.
  storage()->UpdateProfilesOrderPref(index, index);

  // No changes expected with the initial value.
  EXPECT_EQ(
      EntriesToKeys(storage()->GetAllProfilesAttributesSortedForDisplay()),
      initial_keys_order);
  histogram_tester.ExpectUniqueSample("Profile.ProfilesOrderChanged", true, 0u);
}

class ProfileAttributesStorageTestWithProfileReorderingParam
    : public base::test::WithFeatureOverride,
      public ProfileAttributesStorageTest {
 public:
  ProfileAttributesStorageTestWithProfileReorderingParam()
      : base::test::WithFeatureOverride(kProfilesReordering) {}
};

// In this test we are checking the order of which the method
// `GetAllProfilesAttributesSortedByLocalProfileNameWithCheck()` based on the
// feature flag `kProfilesReordering`. When the feature is on, we expect the
// order to be the same as the order of profile insertion. When the feature is
// off, we expect the order to be alphabetically sorted based on the profile
// name.
TEST_P(ProfileAttributesStorageTestWithProfileReorderingParam,
       ProfileOrderWith_GetAllProfilesAttributesSortedWithCheck) {
  DisableObserver();

  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());
  EXPECT_EQ(0U,
            storage()
                ->GetAllProfilesAttributesSortedByLocalProfileNameWithCheck()
                .size());

  const std::u16string profile1(u"D");
  const std::u16string profile2(u"C");
  const std::u16string profile3(u"B");

  // Add two initial profiles "D" and "B".
  AddSimpleTestingProfileWithName(profile1);
  AddSimpleTestingProfileWithName(profile3);

  {
    auto sorted_entries =
        storage()->GetAllProfilesAttributesSortedByLocalProfileNameWithCheck();
    ASSERT_EQ(2U, sorted_entries.size());
    if (IsParamFeatureEnabled()) {
      EXPECT_EQ(profile1, sorted_entries[0]->GetLocalProfileName());
      EXPECT_EQ(profile3, sorted_entries[1]->GetLocalProfileName());
    } else {
      EXPECT_EQ(profile3, sorted_entries[0]->GetLocalProfileName());
      EXPECT_EQ(profile1, sorted_entries[1]->GetLocalProfileName());
    }
  }

  // Add a third profile "C".
  AddSimpleTestingProfileWithName(profile2);

  {
    auto sorted_entries =
        storage()->GetAllProfilesAttributesSortedByLocalProfileNameWithCheck();
    ASSERT_EQ(3U, sorted_entries.size());
    if (IsParamFeatureEnabled()) {
      EXPECT_EQ(profile1, sorted_entries[0]->GetLocalProfileName());
      EXPECT_EQ(profile3, sorted_entries[1]->GetLocalProfileName());
      EXPECT_EQ(profile2, sorted_entries[2]->GetLocalProfileName());
    } else {
      EXPECT_EQ(profile3, sorted_entries[0]->GetLocalProfileName());
      EXPECT_EQ(profile2, sorted_entries[1]->GetLocalProfileName());
      EXPECT_EQ(profile1, sorted_entries[2]->GetLocalProfileName());
    }
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ProfileAttributesStorageTestWithProfileReorderingParam);
