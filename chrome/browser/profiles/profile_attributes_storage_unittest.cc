// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <unordered_set>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/profile_metrics/state.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/native_theme/native_theme.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/signin/profile_colors_util.h"
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

#define TEST_STRING16_ACCESSORS(entry_type, entry, member) \
    TEST_ACCESSORS(entry_type, entry, member, \
        base::ASCIIToUTF16("first_" #member "_value"), \
        base::ASCIIToUTF16("second_" #member "_value"));

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
};
}  // namespace

class ProfileAttributesStorageTest : public testing::Test {
 public:
  ProfileAttributesStorageTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileAttributesStorageTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    VerifyAndResetCallExpectations();
    EnableObserver();
  }

  void TearDown() override {
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
  }

  void EnableObserver() { storage()->AddObserver(&observer_); }
  void DisableObserver() { storage()->RemoveObserver(&observer_); }

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
    return profile_info_cache();
  }

  ProfileInfoCache* profile_info_cache() {
    return testing_profile_manager_.profile_info_cache();
  }

  ProfileAttributesTestObserver& observer() { return observer_; }

  void AddTestingProfile() {
    size_t number_of_profiles = storage()->GetNumberOfProfiles();

    base::FilePath profile_path = GetProfilePath(
        base::StringPrintf("testing_profile_path%" PRIuS, number_of_profiles));

    EXPECT_CALL(observer(), OnProfileAdded(profile_path))
        .Times(1)
        .RetiresOnSaturation();

    storage()->AddProfile(
        profile_path,
        base::ASCIIToUTF16(base::StringPrintf("testing_profile_name%" PRIuS,
                                              number_of_profiles)),
        base::StringPrintf("testing_profile_gaia%" PRIuS, number_of_profiles),
        base::ASCIIToUTF16(base::StringPrintf("testing_profile_user%" PRIuS,
                                              number_of_profiles)),
        true, number_of_profiles, std::string(""), EmptyAccountId());

    EXPECT_EQ(number_of_profiles + 1, storage()->GetNumberOfProfiles());
  }

  TestingProfileManager testing_profile_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ProfileAttributesTestObserver observer_;
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

  storage()->AddProfile(
      GetProfilePath("new_profile_path_1"), u"new_profile_name_1",
      std::string("new_profile_gaia_1"), u"new_profile_username_1", true, 1,
      std::string(""), EmptyAccountId());

  VerifyAndResetCallExpectations();
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("new_profile_path_1"));
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(u"new_profile_name_1", entry->GetName());
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

TEST_F(ProfileAttributesStorageTest, MultipleProfiles) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  for (size_t i = 0; i < 5; ++i) {
    AddTestingProfile();
    EXPECT_EQ(i + 1, storage()->GetNumberOfProfiles());
    EXPECT_EQ(i + 1, storage()->GetAllProfilesAttributes().size());
    EXPECT_EQ(i + 1, storage()->GetAllProfilesAttributesSortedByName().size());
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
  for (auto* entry : entries) {
    EXPECT_NE(GetProfilePath("testing_profile_path0"), entry->GetPath());
  }
}

TEST_F(ProfileAttributesStorageTest, InitialValues) {
  AddTestingProfile();

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(GetProfilePath("testing_profile_path0"), entry->GetPath());
  EXPECT_EQ(u"testing_profile_name0", entry->GetName());
  EXPECT_EQ(std::string("testing_profile_gaia0"), entry->GetGAIAId());
  EXPECT_EQ(u"testing_profile_user0", entry->GetUserName());
  EXPECT_EQ(0U, entry->GetAvatarIconIndex());
  EXPECT_EQ(std::string(), entry->GetSupervisedUserId());
  EXPECT_EQ(std::string(), entry->GetHostedDomain());
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

  EXPECT_CALL(observer(), OnProfileNameChanged(path, _)).Times(2);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsUsingDefaultName);
  VerifyAndResetCallExpectations();

  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsUsingDefaultAvatar);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsAuthError);
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

  // If previous data is not there, setters should returns true even if the
  // defaults (empty string, 0.0, or false) are written.
  EXPECT_TRUE(entry->SetString("test1", std::string()));
  EXPECT_TRUE(entry->SetString16("test2", std::u16string()));
  EXPECT_TRUE(entry->SetDouble("test3", 0.0));
  EXPECT_TRUE(entry->SetBool("test4", false));

  // If previous data is in a wrong type, setters should returns true even if
  // the defaults (empty string, 0.0, or false) are written.
  EXPECT_TRUE(entry->SetString("test3", std::string()));
  EXPECT_TRUE(entry->SetString16("test4", std::u16string()));
  EXPECT_TRUE(entry->SetDouble("test1", 0.0));
  EXPECT_TRUE(entry->SetBool("test2", false));
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
  base::Time lower_bound = base::Time::Now() - base::TimeDelta::FromSeconds(1);
  entry->SetActiveTimeToNow();
  base::Time upper_bound = base::Time::Now() + base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(entry->IsDouble(kActiveTimeKey));
  EXPECT_LE(lower_bound, entry->GetActiveTime());
  EXPECT_GE(upper_bound, entry->GetActiveTime());

  // If the active time was less than one hour ago, SetActiveTimeToNow should do
  // nothing.
  base::Time past = base::Time::Now() - base::TimeDelta::FromMinutes(10);
  lower_bound = past - base::TimeDelta::FromSeconds(1);
  upper_bound = past + base::TimeDelta::FromSeconds(1);
  ASSERT_TRUE(entry->SetDouble(kActiveTimeKey, past.ToDoubleT()));
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

TEST_F(ProfileAttributesStorageTest, SupervisedUsersAccessors) {
  AddTestingProfile();

  base::FilePath path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"));
  ASSERT_NE(entry, nullptr);

  entry->SetSupervisedUserId("");
  ASSERT_FALSE(entry->IsSupervised());
  ASSERT_FALSE(entry->IsChild());

  EXPECT_CALL(observer(), OnProfileSupervisedUserIdChanged(path)).Times(1);
  entry->SetSupervisedUserId("some_supervised_user_id");
  VerifyAndResetCallExpectations();
  ASSERT_TRUE(entry->IsSupervised());
  ASSERT_FALSE(entry->IsChild());

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  EXPECT_CALL(observer(), OnProfileSupervisedUserIdChanged(path)).Times(1);
  entry->SetSupervisedUserId(supervised_users::kChildAccountSUID);
  VerifyAndResetCallExpectations();
  ASSERT_TRUE(entry->IsSupervised());
  ASSERT_TRUE(entry->IsChild());
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
}

TEST_F(ProfileAttributesStorageTest, ReSortTriggered) {
  DisableObserver();  // No need to test observers in this test.

  storage()->AddProfile(GetProfilePath("alpha_path"), u"alpha",
                        std::string("alpha_gaia"), u"alpha_username", true, 1,
                        std::string(""), EmptyAccountId());

  storage()->AddProfile(GetProfilePath("lima_path"), u"lima",
                        std::string("lima_gaia"), u"lima_username", true, 1,
                        std::string(""), EmptyAccountId());

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(GetProfilePath("alpha_path"));
  ASSERT_NE(entry, nullptr);

  // Trigger a ProfileInfoCache re-sort.
  entry->SetLocalProfileName(u"zulu_name",
                             /*is_default_name=*/false);
  EXPECT_EQ(GetProfilePath("alpha_path"), entry->GetPath());
}

TEST_F(ProfileAttributesStorageTest, RemoveOtherProfile) {
  AddTestingProfile();
  AddTestingProfile();
  AddTestingProfile();

  EXPECT_EQ(3U, storage()->GetNumberOfProfiles());

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

  // Deleting through the ProfileInfoCache should be reflected in the
  // ProfileAttributesStorage as well.
  AddCallExpectationsForRemoveProfile(2);
  profile_info_cache()->RemoveProfile(
      GetProfilePath("testing_profile_path2"));
  VerifyAndResetCallExpectations();
  second_entry = storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path2"));
  ASSERT_EQ(second_entry, nullptr);
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

  // The ProfileInfoCache should also reflect the changes and its changes
  // should be reflected by the ProfileAttributesStorage.
  EXPECT_EQ(u"NewName", second_entry->GetName());

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
    storage()->AddProfile(profile_path, std::u16string(), std::string(),
                          std::u16string(), false, icon_index, std::string(),
                          EmptyAccountId());
    VerifyAndResetCallExpectations();
  }

  for (int iter = 0; iter < num_iterations; ++iter) {
    // All icons are used up, expect any valid icon.
    ASSERT_TRUE(profiles::IsModernAvatarIconIndex(
        storage()->ChooseAvatarIconIndexForNewProfile()));
  }
}

TEST_F(ProfileAttributesStorageTest, ProfileForceSigninLock) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(true);

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
  entry->SetIsSigninRequired(false);
  VerifyAndResetCallExpectations();
  ASSERT_FALSE(entry->IsSigninRequired());
}

// Avatar icons not used on Android.
#if !defined(OS_ANDROID)
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
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileAttributesStorageTest, DownloadHighResAvatarTest) {
  storage()->set_disable_avatar_download_for_testing(false);

  const size_t kIconIndex = 0;
  base::FilePath icon_path =
      profiles::GetPathOfHighResAvatarAtIndex(kIconIndex);

  ASSERT_EQ(0U, storage()->GetNumberOfProfiles());
  base::FilePath profile_path = GetProfilePath("path_1");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  storage()->AddProfile(profile_path, u"name_1", std::string(),
                        std::u16string(), false, kIconIndex, std::string(),
                        EmptyAccountId());
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
  // image in the cache.
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
  storage()->AddProfile(profile_path, u"name_1", std::string(),
                        std::u16string(), false, kIconIndex, std::string(),
                        EmptyAccountId());
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
  base::WriteFile(icon_path, bitmap, sizeof(bitmap));
  ASSERT_TRUE(base::PathExists(icon_path));

  // Add a new profile.
  ASSERT_EQ(0U, storage()->GetNumberOfProfiles());
  base::FilePath profile_path = GetProfilePath("path_1");
  EXPECT_CALL(observer(), OnProfileAdded(profile_path)).Times(1);
  storage()->AddProfile(profile_path, u"name_1", std::string(),
                        std::u16string(), false, kIconIndex, std::string(),
                        EmptyAccountId());
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
  histogram_tester.ExpectTotalCount("Profile.State.Avatar_All", 5);
  histogram_tester.ExpectTotalCount("Profile.State.Avatar_ActiveMultiProfile",
                                    5);

  // Other user segments get 0 records.
  histogram_tester.ExpectTotalCount("Profile.State.Avatar_SingleProfile", 0);
  histogram_tester.ExpectTotalCount("Profile.State.Avatar_LatentMultiProfile",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Profile.State.Avatar_LatentMultiProfileActive", 0);
  histogram_tester.ExpectTotalCount(
      "Profile.State.Avatar_LatentMultiProfileOthers", 0);
}

// On Android (at least on KitKat), all profiles are considered active (because
// ActiveTime is not set in production). Thus, these test does not work.
#if !defined(OS_ANDROID)
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
  histogram_tester.ExpectTotalCount("Profile.State.Name_All", 5);
  histogram_tester.ExpectTotalCount("Profile.State.Name_LatentMultiProfile", 5);
  histogram_tester.ExpectTotalCount(
      "Profile.State.Name_LatentMultiProfileActive", 1);
  histogram_tester.ExpectTotalCount(
      "Profile.State.Name_LatentMultiProfileOthers", 4);

  // Other user segments get 0 records.
  histogram_tester.ExpectTotalCount("Profile.State.Name_SingleProfile", 0);
  histogram_tester.ExpectTotalCount("Profile.State.Name_ActiveMultiProfile", 0);
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
#if !defined(OS_ANDROID)
TEST_F(ProfileAttributesStorageTest, ProfileThemeColors) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kNewProfilePicker);
  AddTestingProfile();
  base::FilePath profile_path = GetProfilePath("testing_profile_path0");

  ProfileAttributesEntry* entry =
      storage()->GetProfileAttributesWithPath(profile_path);
  ASSERT_NE(entry, nullptr);
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  entry->SetAvatarIconIndex(profiles::GetPlaceholderAvatarIndex());
  VerifyAndResetCallExpectations();

  EXPECT_EQ(entry->GetProfileThemeColors(),
            GetDefaultProfileThemeColors(false));

  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(true);
  EXPECT_EQ(entry->GetProfileThemeColors(), GetDefaultProfileThemeColors(true));
  EXPECT_NE(entry->GetProfileThemeColors(),
            GetDefaultProfileThemeColors(false));

  ProfileThemeColors colors = {SK_ColorTRANSPARENT, SK_ColorBLACK,
                               SK_ColorWHITE};
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileThemeColorsChanged(profile_path)).Times(1);
  entry->SetProfileThemeColors(colors);
  EXPECT_EQ(entry->GetProfileThemeColors(), colors);
  VerifyAndResetCallExpectations();

  // Colors shouldn't change after switching back to the light mode.
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(false);
  EXPECT_EQ(entry->GetProfileThemeColors(), colors);

  // base::nullopt resets the colors to default.
  EXPECT_CALL(observer(), OnProfileAvatarChanged(profile_path)).Times(1);
  EXPECT_CALL(observer(), OnProfileThemeColorsChanged(profile_path)).Times(1);
  entry->SetProfileThemeColors(base::nullopt);
  EXPECT_EQ(entry->GetProfileThemeColors(),
            GetDefaultProfileThemeColors(false));
  VerifyAndResetCallExpectations();
}
#endif
