// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/config_source.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_test_utils.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace parent_access {

// Stores information about results of the access code validation.
struct CodeValidationResults {
  // Number of successful access code validations.
  int success_count = 0;

  // Number of attempts when access code validation failed.
  int failure_count = 0;
};

// ParentAccessServiceObserver implementation used for tests.
class TestParentAccessServiceObserver : public ParentAccessService::Observer {
 public:
  explicit TestParentAccessServiceObserver(const AccountId& account_id)
      : account_id_(account_id) {}

  TestParentAccessServiceObserver(const TestParentAccessServiceObserver&) =
      delete;
  TestParentAccessServiceObserver& operator=(
      const TestParentAccessServiceObserver&) = delete;

  ~TestParentAccessServiceObserver() override = default;

  void OnAccessCodeValidation(ParentCodeValidationResult result,
                              std::optional<AccountId> account_id) override {
    ASSERT_TRUE(account_id);
    EXPECT_EQ(account_id_, account_id.value());
    result == ParentCodeValidationResult::kValid
        ? ++validation_results_.success_count
        : ++validation_results_.failure_count;
  }

  CodeValidationResults validation_results_;

 private:
  const AccountId account_id_;
};

class ParentAccessServiceTest : public MixinBasedInProcessBrowserTest {
 public:
  ParentAccessServiceTest()
      : test_observer_(std::make_unique<TestParentAccessServiceObserver>(
            logged_in_user_mixin_.GetAccountId())) {}

  ParentAccessServiceTest(const ParentAccessServiceTest&) = delete;
  ParentAccessServiceTest& operator=(const ParentAccessServiceTest&) = delete;

  ~ParentAccessServiceTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_NO_FATAL_FAILURE(GetTestAccessCodeValues(&test_values_));
    ParentAccessService::Get().AddObserver(test_observer_.get());
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
  }

  void TearDownOnMainThread() override {
    ParentAccessService::Get().RemoveObserver(test_observer_.get());
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Updates the policy containing the Parent Access Code config.
  void UpdatePolicy(const base::Value& dict) {
    std::string config_string;
    base::JSONWriter::Write(dict, &config_string);

    logged_in_user_mixin_.GetUserPolicyMixin()
        ->RequestPolicyUpdate()
        ->policy_payload()
        ->mutable_parentaccesscodeconfig()
        ->set_value(config_string);

    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    EXPECT_TRUE(user_manager->GetActiveUser()->IsChild());
    Profile* child_profile =
        ProfileHelper::Get()->GetProfileByUser(user_manager->GetActiveUser());

    logged_in_user_mixin_.GetUserPolicyTestHelper()->RefreshPolicyAndWait(
        child_profile);
  }

  // Performs |code| validation on ParentAccessService singleton using the
  // |validation time| and returns the result.
  ParentCodeValidationResult ValidateAccessCode(const std::string& code,
                                                base::Time validation_time) {
    return ParentAccessService::Get().ValidateParentAccessCode(
        logged_in_user_mixin_.GetAccountId(), code, validation_time);
  }

  // Checks if ParentAccessServiceObserver and ValidateParentAccessCodeCallback
  // were called as intended. Expects |success_count| of successful access code
  // validations and |failure_count| of failed validation attempts.
  void ExpectResults(int success_count, int failure_count) {
    EXPECT_EQ(success_count, test_observer_->validation_results_.success_count);
    EXPECT_EQ(failure_count, test_observer_->validation_results_.failure_count);
  }

  AccessCodeValues test_values_;
  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_, /*test_base=*/this,
                                          embedded_test_server(),
                                          LoggedInUserMixin::LogInType::kChild};
  std::unique_ptr<TestParentAccessServiceObserver> test_observer_;
};

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoConfigAvailable) {
  auto test_value = test_values_.begin();
  EXPECT_EQ(ParentCodeValidationResult::kNoConfig,
            ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(0, 1);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoValidConfigAvailable) {
  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetInvalidTestConfig(), GetInvalidTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_EQ(ParentCodeValidationResult::kInvalid,
            ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(0, 1);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, ValidationWithFutureConfig) {
  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetDefaultTestConfig(), GetInvalidTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_EQ(ParentCodeValidationResult::kValid,
            ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(1, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, ValidationWithCurrentConfig) {
  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_EQ(ParentCodeValidationResult::kValid,
            ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(1, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, ValidationWithOldConfig) {
  std::vector<AccessCodeConfig> old_configs;
  old_configs.emplace_back(GetInvalidTestConfig());
  old_configs.emplace_back(GetDefaultTestConfig());
  UpdatePolicy(PolicyFromConfigs(GetInvalidTestConfig(), GetInvalidTestConfig(),
                                 old_configs));

  auto test_value = test_values_.begin();
  EXPECT_EQ(ParentCodeValidationResult::kValid,
            ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(1, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, MultipleValidationAttempts) {
  AccessCodeValues::iterator test_value = test_values_.begin();

  // No config - validation should fail.
  EXPECT_EQ(ParentCodeValidationResult::kNoConfig,
            ValidateAccessCode(test_value->second, test_value->first));

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  // Valid config - validation should pass.
  for (auto& value : test_values_) {
    EXPECT_EQ(ParentCodeValidationResult::kValid,
              ValidateAccessCode(value.second, value.first));
  }

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetInvalidTestConfig(), {}));

  // Invalid config - validation should fail.
  EXPECT_EQ(ParentCodeValidationResult::kInvalid,
            ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(test_values_.size(), 2);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoObserver) {
  ParentAccessService::Get().RemoveObserver(test_observer_.get());

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  auto test_value = test_values_.begin();
  EXPECT_EQ(ParentCodeValidationResult::kValid,
            ValidateAccessCode(test_value->second, test_value->first));

  ExpectResults(0, 0);
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, NoAccountId) {
  ParentAccessService::Get().RemoveObserver(test_observer_.get());

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  auto test_value = test_values_.begin();

  EXPECT_EQ(ParentCodeValidationResult::kValid,
            ParentAccessService::Get().ValidateParentAccessCode(
                EmptyAccountId(), test_value->second, test_value->first));
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest, InvalidAccountId) {
  ParentAccessService::Get().RemoveObserver(test_observer_.get());

  UpdatePolicy(
      PolicyFromConfigs(GetInvalidTestConfig(), GetDefaultTestConfig(), {}));

  auto test_value = test_values_.begin();

  AccountId other_child = AccountId::FromUserEmail("otherchild@gmail.com");
  EXPECT_EQ(ParentCodeValidationResult::kNoConfig,
            ParentAccessService::Get().ValidateParentAccessCode(
                other_child, test_value->second, test_value->first));
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest,
                       ChildDeviceOwner_IsApprovalRequired) {
  auto* const user_manager =
      static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  user_manager->SetOwnerId(logged_in_user_mixin_.GetAccountId());

  // No configuration available - reauth does not require PAC.
  // Login screen.
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kAddUser));
  EXPECT_FALSE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kReauth));
  // In session, because child user is logged in the test fixture.
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUnlockTimeLimits));
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kUpdateClock));
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUpdateTimezone));

  // Configuration available.
  UpdatePolicy(
      PolicyFromConfigs(GetDefaultTestConfig(), GetDefaultTestConfig(), {}));
  // Login screen.
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kAddUser));
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kReauth));
  // In session, because child user is logged in the test fixture.
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUnlockTimeLimits));
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kUpdateClock));
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUpdateTimezone));
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest,
                       RegularDeviceOwner_IsApprovalRequired) {
  auto* const user_manager =
      static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  const AccountId regular_user = AccountId::FromUserEmail("regular@gmail.com");
  user_manager->AddUser(regular_user);
  user_manager->SetOwnerId(regular_user);

  // No configuration available - reauth does not require PAC.
  // Login screen.
  EXPECT_FALSE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kAddUser));
  EXPECT_FALSE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kReauth));
  // In session. Child user is logged in the test fixture.
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUnlockTimeLimits));
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kUpdateClock));
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUpdateTimezone));

  // Configuration available.
  UpdatePolicy(
      PolicyFromConfigs(GetDefaultTestConfig(), GetDefaultTestConfig(), {}));
  // Login screen.
  EXPECT_FALSE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kAddUser));
  EXPECT_FALSE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kReauth));
  // In session, because child user is logged in the test fixture.
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUnlockTimeLimits));
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kUpdateClock));
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUpdateTimezone));
}

}  // namespace parent_access
}  // namespace ash
