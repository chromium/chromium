// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_util.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

namespace crosapi {

namespace {
std::unique_ptr<policy::UserCloudPolicyManagerAsh> CreateUserCloudPolicyManager(
    Profile* profile,
    AccountId account_id,
    std::unique_ptr<policy::CloudPolicyStore> store) {
  auto fatal_error_callback = []() {
    LOG(ERROR) << "Fatal error: policy could not be loaded";
  };
  return std::make_unique<policy::UserCloudPolicyManagerAsh>(
      profile, std::move(store),
      std::make_unique<policy::MockCloudExternalDataManager>(),
      /*component_policy_cache_path=*/base::FilePath(),
      policy::UserCloudPolicyManagerAsh::PolicyEnforcement::kPolicyRequired,
      g_browser_process->local_state(),
      /*policy_refresh_timeout=*/base::Minutes(1),
      base::BindOnce(fatal_error_callback), account_id,
      base::SingleThreadTaskRunner::GetCurrentDefault());
}
}  // namespace

class CrosapiUtilTest : public testing::Test {
 public:
  CrosapiUtilTest() = default;
  ~CrosapiUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    user_manager::UserManagerImpl::RegisterProfilePrefs(
        pref_service_.registry());
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);

    auto cloud_policy_store = std::make_unique<policy::MockCloudPolicyStore>();
    cloud_policy_store_ = cloud_policy_store.get();
    testing_profile_->SetUserCloudPolicyManagerAsh(CreateUserCloudPolicyManager(
        testing_profile_,
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName),
        std::move(cloud_policy_store)));
  }

  void TearDown() override {
    for (const auto& account_id : profile_created_accounts_) {
      fake_user_manager_->OnUserProfileWillBeDestroyed(account_id);
    }

    cloud_policy_store_ = nullptr;
    testing_profile_ = nullptr;
    profile_manager_.reset();
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));
    fake_user_manager_->OnUserProfileCreated(account_id, &pref_service_);
    profile_created_accounts_.push_back(account_id);
  }

  policy::MockCloudPolicyStore* GetCloudPolicyStore() {
    return cloud_policy_store_;
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> testing_profile_;
  TestingPrefServiceSimple pref_service_;
  std::vector<AccountId> profile_created_accounts_;
  raw_ptr<policy::MockCloudPolicyStore> cloud_policy_store_ = nullptr;
};

TEST_F(CrosapiUtilTest, IsSigninProfileOrBelongsToAffiliatedUserSigninProfile) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(ash::kSigninBrowserContextBaseName));
  std::unique_ptr<Profile> signin_profile = builder.Build();

  EXPECT_TRUE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      signin_profile.get()));
}

TEST_F(CrosapiUtilTest, IsSigninProfileOrBelongsToAffiliatedUserOffTheRecord) {
  Profile* otr_profile = testing_profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  EXPECT_FALSE(
      browser_util::IsSigninProfileOrBelongsToAffiliatedUser(otr_profile));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserAffiliatedUser) {
  AccountId account_id =
      AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName);
  fake_user_manager_->AddUserWithAffiliation(account_id,
                                             /*is_affiliated=*/true);
  fake_user_manager_->UserLoggedIn(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));

  EXPECT_TRUE(
      browser_util::IsSigninProfileOrBelongsToAffiliatedUser(testing_profile_));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserNotAffiliatedUser) {
  AddRegularUser(TestingProfile::kDefaultProfileUserName);

  EXPECT_FALSE(
      browser_util::IsSigninProfileOrBelongsToAffiliatedUser(testing_profile_));
}

TEST_F(CrosapiUtilTest,
       IsSigninProfileOrBelongsToAffiliatedUserLockScreenProfile) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(ash::kLockScreenBrowserContextBaseName));
  std::unique_ptr<Profile> lock_screen_profile = builder.Build();

  EXPECT_FALSE(browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
      lock_screen_profile.get()));
}

}  // namespace crosapi
