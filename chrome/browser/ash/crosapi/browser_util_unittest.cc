// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using crosapi::browser_util::LacrosAvailability;
using crosapi::browser_util::LacrosLaunchSwitchSource;
using crosapi::browser_util::LacrosSelection;
using user_manager::User;
using version_info::Channel;

namespace crosapi {

// This implementation of RAII for LacrosAvailability is to make it easy reset
// the state between runs.
class ScopedLacrosAvailabilityCache {
 public:
  explicit ScopedLacrosAvailabilityCache(
      LacrosAvailability lacros_launch_switch) {
    SetLacrosAvailability(lacros_launch_switch);
  }
  ScopedLacrosAvailabilityCache(const ScopedLacrosAvailabilityCache&) = delete;
  ScopedLacrosAvailabilityCache& operator=(
      const ScopedLacrosAvailabilityCache&) = delete;
  ~ScopedLacrosAvailabilityCache() {
    browser_util::ClearLacrosAvailabilityCacheForTest();
  }

 private:
  void SetLacrosAvailability(LacrosAvailability lacros_availability) {
    policy::PolicyMap policy;
    policy.Set(
        policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value(GetLacrosAvailabilityPolicyName(lacros_availability)),
        /*external_data_fetcher=*/nullptr);
    browser_util::CacheLacrosAvailability(policy);
  }
};

class BrowserUtilTest : public testing::Test {
 public:
  BrowserUtilTest() = default;
  ~BrowserUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));
    browser_util::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &testing_profile_);
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  TestingPrefServiceSimple pref_service_;
};

class LacrosSupportBrowserUtilTest : public BrowserUtilTest {
 public:
  LacrosSupportBrowserUtilTest() {
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kLacrosSupport);
  }
  ~LacrosSupportBrowserUtilTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(hidehiko): Replace with ScopedTestingLocalState.
class ScopedLocalState {
 public:
  explicit ScopedLocalState(PrefService* local_state) {
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state);
  }

  ~ScopedLocalState() {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }
};

TEST_F(LacrosSupportBrowserUtilTest, LacrosEnabledByFlag) {
  AddRegularUser("user@test.com");

  // Lacros is initially disabled.
  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  // Enabling the flag enables Lacros.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, LacrosDisabledWithoutMigration) {
  // This sets `g_browser_process->local_state()` which activates the check
  // `IsProfileMigrationCompletedForUser()` inside `IsLacrosEnabled()`.
  ScopedLocalState scoped_local_state(&pref_service_);

  // Note that disabling lacros is only enabled for Googlers at the moment.
  // TODO(crbug.com/1266669): Once profile migration is enabled for
  // non-googlers, add a @test.com account instead.
  AddRegularUser("user@google.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);

  // Lacros is now enabled for profile migration to happen.
  EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
      user, browser_util::PolicyInitState::kAfterInit));
  // Since profile migration hasn't been marked as completed, this returns
  // false.
  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  browser_util::SetProfileMigrationCompletedForUser(&pref_service_,
                                                    user->username_hash());

  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, IsLacrosEnabledForMigrationBeforePolocyInit) {
  // This sets `g_browser_process->local_state()` which activates the check
  // `IsProfileMigrationCompletedForUser()` inside `IsLacrosEnabled()`.
  ScopedLocalState scoped_local_state(&pref_service_);

  // Add an user.
  AddRegularUser("user@test.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);

  // Lacros is not enabled yet for profile migration to happen.
  EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
      user, browser_util::PolicyInitState::kBeforeInit));

  // Sets command line flag to emulate the situation where the Chrome
  // restart happens.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(browser_util::kLacrosAvailabilityPolicySwitch,
                             browser_util::kLacrosAvailabilityPolicySideBySide);

  EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
      user, browser_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserUtilTest, LacrosCrosTeamRollout) {
  AddRegularUser("user@google.com");
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kSideBySide);
    EXPECT_EQ(browser_util::GetCachedLacrosAvailabilityForTesting(),
              LacrosAvailability::kSideBySide);
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {browser_util::kLacrosGooglePolicyRollout});

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kSideBySide);
    EXPECT_EQ(browser_util::GetCachedLacrosAvailabilityForTesting(),
              LacrosAvailability::kUserChoice);
  }
}

TEST_F(BrowserUtilTest, LacrosEnabled) {
  AddRegularUser("user@test.com");

  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, ManagedAccountLacros) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
  }
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kSideBySide);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosPrimary);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, BlockedForChildUser) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AccountId account_id = AccountId::FromUserEmail("user@test.com");
  const User* user = fake_user_manager_->AddChildUser(account_id);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/true);
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
}

TEST_F(LacrosSupportBrowserUtilTest, AshWebBrowserEnabled) {
  base::test::ScopedFeatureList feature_list;
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  // Lacros is not allowed.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);

    EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  }

  // Lacros is allowed but not enabled.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  }

  // Lacros is allowed and enabled by flag.
  {
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  }

  // Lacros is allowed and enabled by policy.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kSideBySide);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  }
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosPrimary);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  }
}

TEST_F(BrowserUtilTest, IsAshWebBrowserDisabled) {
  base::test::ScopedFeatureList feature_list;
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);

  // Lacros is allowed and enabled and is the only browser by policy.

  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
}

TEST_F(BrowserUtilTest, IsAshWebBrowserDisabledByFlags) {
  AddRegularUser("user@test.com");
  { EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled()); }

  // Just enabling LacrosOnly feature is not enough.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  }

  // LacrosSupport only is not enough.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {chromeos::features::kLacrosOnly, chromeos::features::kLacrosSupport},
        {});
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
  }

  // All, LacrosOnly, LacrosPrimary and LacrosSupport are needed.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {chromeos::features::kLacrosOnly, chromeos::features::kLacrosPrimary,
         chromeos::features::kLacrosSupport},
        {});
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
  }
}

TEST_F(LacrosSupportBrowserUtilTest, LacrosPrimaryBrowserByFlags) {
  AddRegularUser("user@test.com");
  { EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser()); }

  // Just enabling LacrosPrimary feature is not enough.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosPrimary);
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser());
  }

  // Both LacrosPrimary and LacrosSupport are needed.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({chromeos::features::kLacrosPrimary,
                                   chromeos::features::kLacrosSupport},
                                  {});
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser());
  }
}

TEST_F(BrowserUtilTest, LacrosPrimaryBrowser) {
  AddRegularUser("user@test.com");

  // Currently, only developer build can use Lacros as a primary
  // web browser.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kLacrosPrimary, chromeos::features::kLacrosSupport},
      {});
  EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser());
}

TEST_F(BrowserUtilTest, LacrosPrimaryBrowserAllowed) {
  AddRegularUser("user@test.com");

  EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed());
}

TEST_F(BrowserUtilTest, ManagedAccountLacrosPrimary) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  AddRegularUser("user@managedchrome.com");
  testing_profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      true);

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowserAllowed());
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser());
  }

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kSideBySide);
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed());
    EXPECT_FALSE(browser_util::IsLacrosPrimaryBrowser());
  }

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosPrimary);
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed());
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser());
  }

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowserAllowed());
    EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser());
  }
}

TEST_F(BrowserUtilTest, MetadataMissing) {
  EXPECT_FALSE(browser_util::DoesMetadataSupportNewAccountManager(nullptr));
}

TEST_F(BrowserUtilTest, MetadataMissingVersion) {
  std::string json_string = R"###(
   {
     "content": {
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataVersionBadFormat) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469"
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataOldVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataNewVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "9999.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_TRUE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, GetMissingDataVer) {
  std::string user_id_hash = "1234";
  base::Version version =
      browser_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_FALSE(version.IsValid());
}

TEST_F(BrowserUtilTest, GetCorruptDataVer) {
  base::DictionaryValue dictionary_value;
  std::string user_id_hash = "1234";
  dictionary_value.SetStringKey(user_id_hash, "corrupted");
  pref_service_.Set(browser_util::kDataVerPref, dictionary_value);
  base::Version version =
      browser_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_FALSE(version.IsValid());
}

TEST_F(BrowserUtilTest, GetDataVer) {
  base::DictionaryValue dictionary_value;
  std::string user_id_hash = "1234";
  base::Version version{"1.1.1.1"};
  dictionary_value.SetStringKey(user_id_hash, version.GetString());
  pref_service_.Set(browser_util::kDataVerPref, dictionary_value);

  base::Version result_version =
      browser_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_EQ(version, result_version);
}

TEST_F(BrowserUtilTest, RecordDataVer) {
  std::string user_id_hash = "1234";
  base::Version version{"1.1.1.1"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash, version);

  base::Value expected{base::Value::Type::DICTIONARY};
  expected.SetStringKey(user_id_hash, version.GetString());
  const base::Value* dict =
      pref_service_.GetDictionary(browser_util::kDataVerPref);
  EXPECT_EQ(*dict, expected);
}

TEST_F(BrowserUtilTest, RecordDataVerOverrides) {
  std::string user_id_hash = "1234";

  base::Version version1{"1.1.1.1"};
  base::Version version2{"1.1.1.2"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash, version1);
  browser_util::RecordDataVer(&pref_service_, user_id_hash, version2);

  base::Value expected{base::Value::Type::DICTIONARY};
  expected.SetStringKey(user_id_hash, version2.GetString());

  const base::Value* dict =
      pref_service_.GetDictionary(browser_util::kDataVerPref);
  EXPECT_EQ(*dict, expected);
}

TEST_F(BrowserUtilTest, RecordDataVerWithMultipleUsers) {
  std::string user_id_hash_1 = "1234";
  std::string user_id_hash_2 = "2345";
  base::Version version1{"1.1.1.1"};
  base::Version version2{"1.1.1.2"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash_1, version1);
  browser_util::RecordDataVer(&pref_service_, user_id_hash_2, version2);

  EXPECT_EQ(version1, browser_util::GetDataVer(&pref_service_, user_id_hash_1));
  EXPECT_EQ(version2, browser_util::GetDataVer(&pref_service_, user_id_hash_2));

  base::Version version3{"3.3.3.3"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash_1, version3);

  base::Value expected{base::Value::Type::DICTIONARY};
  expected.SetStringKey(user_id_hash_1, version3.GetString());
  expected.SetStringKey(user_id_hash_2, version2.GetString());

  const base::Value* dict =
      pref_service_.GetDictionary(browser_util::kDataVerPref);
  EXPECT_EQ(*dict, expected);
}

TEST_F(BrowserUtilTest, IsDataWipeRequiredInvalid) {
  const base::Version data_version;
  const base::Version current{"3"};
  const base::Version required{"2"};

  ASSERT_FALSE(data_version.IsValid());
  EXPECT_TRUE(browser_util::IsDataWipeRequiredForTesting(data_version, current,
                                                         required));
}

TEST_F(BrowserUtilTest, IsDataWipeRequiredFutureVersion) {
  const base::Version data_version{"1"};
  const base::Version current{"2"};
  const base::Version required{"3"};

  EXPECT_FALSE(browser_util::IsDataWipeRequiredForTesting(data_version, current,
                                                          required));
}

TEST_F(BrowserUtilTest, IsDataWipeRequiredSameVersion) {
  const base::Version data_version{"3"};
  const base::Version current{"4"};
  const base::Version required{"3"};

  EXPECT_FALSE(browser_util::IsDataWipeRequiredForTesting(data_version, current,
                                                          required));
}

TEST_F(BrowserUtilTest, IsDataWipeRequired) {
  const base::Version data_version{"1"};
  const base::Version current{"3"};
  const base::Version required{"2"};

  EXPECT_TRUE(browser_util::IsDataWipeRequiredForTesting(data_version, current,
                                                         required));
}

TEST_F(BrowserUtilTest, IsDataWipeRequired2) {
  const base::Version data_version{"1"};
  const base::Version current{"3"};
  const base::Version required{"3"};

  EXPECT_TRUE(browser_util::IsDataWipeRequiredForTesting(data_version, current,
                                                         required));
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlock) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kVersion = "91.0.4457";
  const std::string kContent =
      "{\"content\":{\"version\":\"" + kVersion + "\"}}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_EQ(browser_util::GetRootfsLacrosVersionMayBlock(path),
            base::Version(kVersion));
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingVersion) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "{\"content\":{}}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingContent) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "{}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingFile) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  auto bad_path = tmp_dir.GetPath().Append("file");

  EXPECT_FALSE(
      browser_util::GetRootfsLacrosVersionMayBlock(bad_path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockBadJson) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "!@#$";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, StatefulLacrosSelectionUpdateChannel) {
  // Assert that when no Lacros stability switch is specified, we return the
  // "unknown" channel.
  ASSERT_EQ(Channel::UNKNOWN, browser_util::GetLacrosSelectionUpdateChannel(
                                  LacrosSelection::kStateful));

  // Assert that when a Lacros stability switch is specified, we return the
  // relevant channel name associated to that switch value.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchNative(browser_util::kLacrosStabilitySwitch,
                              browser_util::kLacrosStabilityChannelBeta);
  ASSERT_EQ(Channel::BETA, browser_util::GetLacrosSelectionUpdateChannel(
                               LacrosSelection::kStateful));
  cmdline->RemoveSwitch(browser_util::kLacrosStabilitySwitch);
}

TEST_F(BrowserUtilTest, IsProfileMigrationCompletedForUser) {
  const std::string user_id_hash = "abcd";

  // `IsLacrosDisabledAfterSkippedOrFailedMigration()` should return
  // false by default.
  EXPECT_FALSE(browser_util::IsProfileMigrationCompletedForUser(&pref_service_,
                                                                user_id_hash));

  browser_util::SetProfileMigrationCompletedForUser(&pref_service_,
                                                    user_id_hash);
  EXPECT_TRUE(browser_util::IsProfileMigrationCompletedForUser(&pref_service_,
                                                               user_id_hash));

  browser_util::ClearProfileMigrationCompletedForUser(&pref_service_,
                                                      user_id_hash);
  EXPECT_FALSE(browser_util::IsProfileMigrationCompletedForUser(&pref_service_,
                                                                user_id_hash));
}

TEST_F(BrowserUtilTest, IsAshBrowserSyncEnabled) {
  {
    browser_util::SetLacrosEnabledForTest(false);
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshBrowserSyncEnabled());
  }

  {
    browser_util::SetLacrosEnabledForTest(true);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshBrowserSyncEnabled());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {chromeos::features::kLacrosOnly, chromeos::features::kLacrosPrimary,
         chromeos::features::kLacrosSupport},
        {});
    browser_util::SetLacrosEnabledForTest(false);
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshBrowserSyncEnabled());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {chromeos::features::kLacrosOnly, chromeos::features::kLacrosPrimary,
         chromeos::features::kLacrosSupport},
        {});
    browser_util::SetLacrosEnabledForTest(true);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_FALSE(browser_util::IsAshBrowserSyncEnabled());
  }
}

TEST_F(BrowserUtilTest, GetLacrosLaunchSwitchSource) {
  // If LaunchSwitch is not set, the source is unknown.
  EXPECT_EQ(LacrosLaunchSwitchSource::kUnknown,
            browser_util::GetLacrosLaunchSwitchSource());

  // If the policy says UserChoice, lacros state may be set by user.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    EXPECT_EQ(LacrosLaunchSwitchSource::kPossiblySetByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // The policy can be ignored by command line flag.
  // In the case, it is forced by user. Note that if the flag is set,
  // LacrosAvailability is always kUserChoice.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kLacrosAvailabilityIgnore);
    EXPECT_EQ(LacrosLaunchSwitchSource::kForcedByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // Otherwise, the LaunchSwitch is set by the policy.
  for (const auto launch_switch :
       {LacrosAvailability::kLacrosDisallowed, LacrosAvailability::kSideBySide,
        LacrosAvailability::kLacrosPrimary, LacrosAvailability::kLacrosOnly}) {
    ScopedLacrosAvailabilityCache cache(launch_switch);
    EXPECT_EQ(LacrosLaunchSwitchSource::kForcedByPolicy,
              browser_util::GetLacrosLaunchSwitchSource())
        << static_cast<int>(launch_switch);
  }
}

// Check that the exist configurations used for the Google rollout have the
// precisely intended side-effects.
TEST_F(BrowserUtilTest, LacrosGoogleRolloutUserChoice) {
  AddRegularUser("user@google.com");
  // Lacros availability is set by policy to user choice.
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

  // We enable 3 features: LacrosSupport, LacrosPrimary, LacrosOnly
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kLacrosSupport, chromeos::features::kLacrosPrimary,
       chromeos::features::kLacrosOnly},
      {});

  // Check that Lacros is allowed, enabled, and set to lacros-only.
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
}

TEST_F(BrowserUtilTest, LacrosGoogleRolloutPrimary) {
  AddRegularUser("user@google.com");
  // Lacros availability is set by policy to primary.
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosPrimary);

  // We enable 3 features: LacrosSupport, LacrosPrimary, LacrosOnly
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kLacrosSupport, chromeos::features::kLacrosPrimary,
       chromeos::features::kLacrosOnly},
      {});

  // Check that Lacros is allowed, enabled, and set to lacros-only.
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsLacrosPrimaryBrowser());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
}

}  // namespace crosapi
