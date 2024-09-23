// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/lacros_data_backward_migration_mode_policy_observer.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace crosapi {

class LacrosDataBackwardMigrationModePolicyObserverTest : public testing::Test {
 public:
  LacrosDataBackwardMigrationModePolicyObserverTest() = default;
  LacrosDataBackwardMigrationModePolicyObserverTest(
      const LacrosDataBackwardMigrationModePolicyObserverTest&) = delete;
  LacrosDataBackwardMigrationModePolicyObserverTest& operator=(
      const LacrosDataBackwardMigrationModePolicyObserverTest&) = delete;
  ~LacrosDataBackwardMigrationModePolicyObserverTest() override = default;

  void SetUp() override {
    ash::SessionManagerClient::InitializeFake();
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    // Add primary user.
    test_user_ = fake_user_manager_->AddPublicAccountUser(
        AccountId::FromUserEmailGaiaId("test@test.com", "test_user"));

    ASSERT_TRUE(profile_manager_->SetUp());

    // Set up ProfileHelper so that profile creation is always interpreted
    // as if it is Primary profile creation.
    ash::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
  }

  void TearDown() override {
    primary_profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    ash::SessionManagerClient::Shutdown();
  }

  void CreatePrimaryProfile() {
    if (!primary_profile_) {
      fake_user_manager_->LoginUser(test_user_->GetAccountId(),
                                    /*set_profile_created_flag=*/true);
      fake_user_manager_->SwitchActiveUser(test_user_->GetAccountId());
      primary_profile_ = profile_manager_->CreateTestingProfile("test-profile");
    }
  }

  std::vector<std::string> GetFeatureFlagsForPrimaryUser() {
    auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(
        test_user_->GetAccountId());
    std::vector<std::string> flags;
    if (!ash::FakeSessionManagerClient::Get()->GetFlagsForUser(account_id,
                                                               &flags))
      return {};

    const std::string prefix =
        base::StringPrintf("--%s=", chromeos::switches::kFeatureFlags);
    for (const std::string& flag : flags) {
      if (base::StartsWith(flag, prefix)) {
        std::string_view flag_value(flag);
        flag_value.remove_prefix(prefix.size());
        std::optional<base::Value> parsed = base::JSONReader::Read(flag_value);
        std::vector<std::string> result;
        if (parsed && parsed->is_list()) {
          for (const auto& element : parsed->GetList()) {
            result.push_back(element.GetString());
          }
        }
        return result;
      }
    }

    // Not found.
    return {};
  }

  TestingPrefServiceSimple* local_state() {
    return profile_manager_->local_state()->Get();
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<user_manager::User> test_user_ = nullptr;
  raw_ptr<TestingProfile> primary_profile_ = nullptr;
};

TEST_F(LacrosDataBackwardMigrationModePolicyObserverTest, OnPolicyUpdate) {
  LacrosDataBackwardMigrationModePolicyObserver observer;
  CreatePrimaryProfile();

  {
    auto feature_flags = GetFeatureFlagsForPrimaryUser();
    EXPECT_TRUE(feature_flags.empty());
  }

  local_state()->SetManagedPref(prefs::kLacrosDataBackwardMigrationMode,
                                base::Value("keep_all"));

  {
    auto feature_flags = GetFeatureFlagsForPrimaryUser();
    ASSERT_EQ(1u, feature_flags.size());
    // Please find about_flags.cc for actual mapping of the enum value
    // to the index.
    EXPECT_EQ("lacros-data-backward-migration-policy@4", feature_flags[0]);
  }

  local_state()->SetManagedPref(prefs::kLacrosDataBackwardMigrationMode,
                                base::Value("none"));

  {
    auto feature_flags = GetFeatureFlagsForPrimaryUser();
    ASSERT_EQ(1u, feature_flags.size());
    // Please find about_flags.cc for actual mapping of the enum value
    // to the index.
    EXPECT_EQ("lacros-data-backward-migration-policy@1", feature_flags[0]);
  }
}

TEST_F(LacrosDataBackwardMigrationModePolicyObserverTest,
       AroundPrimaryProfileCreation) {
  LacrosDataBackwardMigrationModePolicyObserver observer;
  {
    auto feature_flags = GetFeatureFlagsForPrimaryUser();
    EXPECT_TRUE(feature_flags.empty());
  }

  local_state()->SetManagedPref(prefs::kLacrosDataBackwardMigrationMode,
                                base::Value("keep_all"));
  // Do not update the feature_flags in session_manger, until primary profile
  // is created.
  {
    auto feature_flags = GetFeatureFlagsForPrimaryUser();
    EXPECT_TRUE(feature_flags.empty());
  }

  CreatePrimaryProfile();

  // After the primary profile creation, then feature flag should set.
  {
    auto feature_flags = GetFeatureFlagsForPrimaryUser();
    ASSERT_EQ(1u, feature_flags.size());
    // Please find about_flags.cc for actual mapping of the enum value
    // to the index.
    EXPECT_EQ("lacros-data-backward-migration-policy@4", feature_flags[0]);
  }
}

}  // namespace crosapi
