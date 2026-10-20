// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/config_source.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_test_utils.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_method_short.h"
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_on_off.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/mojom/date_time_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/new_date_time_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::parent_access {

namespace {

class TestDateTimePage : public settings::date_time::mojom::Page {
 public:
  TestDateTimePage() = default;
  ~TestDateTimePage() override = default;

  mojo::PendingRemote<settings::date_time::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  bool WaitForParentAccessValidation() { return validation_future_.Get(); }

  // settings::date_time::mojom::Page:
  void OnSystemClockCanSetTimeChanged(bool is_allowed) override {}
  void OnParentAccessValidationComplete(bool success) override {
    validation_future_.SetValue(success);
  }

 private:
  base::test::TestFuture<bool> validation_future_;
  mojo::Receiver<settings::date_time::mojom::Page> receiver_{this};
};

}  // namespace

// Stores information about results of the access code validation.
struct CodeValidationResults {
  // Number of successful access code validations.
  int success_count = 0;

  // Number of attempts when access code validation failed.
  int failure_count = 0;
};

constexpr auto kRegularUserAccountId =
    AccountId::Literal::FromUserEmailGaiaId("regular@gmail.com",
                                            GaiaId::Literal("test-gaia"));

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

  void SetUpLocalStatePrefService(PrefService* local_state) override {
    MixinBasedInProcessBrowserTest::SetUpLocalStatePrefService(local_state);
    user_manager::TestHelper::RegisterPersistedUser(*local_state,
                                                    kRegularUserAccountId);
  }

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
    std::string config_string = base::WriteJson(dict).value_or("");

    logged_in_user_mixin_.GetUserPolicyMixin()
        ->RequestPolicyUpdate()
        ->policy_payload()
        ->mutable_parentaccesscodeconfig()
        ->set_value(config_string);

    const session_manager::Session* active_session =
        session_manager::SessionManager::Get()->GetActiveSession();
    ASSERT_TRUE(active_session);
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->FindUser(
            active_session->account_id());
    ASSERT_TRUE(active_user);
    EXPECT_TRUE(active_user->IsChild());
    Profile* child_profile = Profile::FromBrowserContext(
        BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));

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
  user_manager::UserManager::Get()->SetOwnerId(
      logged_in_user_mixin_.GetAccountId());

  // No configuration available - reauth does not require PAC.
  // Login screen.
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kAddUser));
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
  user_manager::UserManager::Get()->SetOwnerId(kRegularUserAccountId);

  // No configuration available - reauth does not require PAC.
  // Login screen.
  EXPECT_FALSE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kAddUser));
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
  // In session, because child user is logged in the test fixture.
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUnlockTimeLimits));
  EXPECT_TRUE(
      ParentAccessService::IsApprovalRequired(SupervisedAction::kUpdateClock));
  EXPECT_TRUE(ParentAccessService::IsApprovalRequired(
      SupervisedAction::kUpdateTimezone));
}

IN_PROC_BROWSER_TEST_F(ParentAccessServiceTest,
                       TimezoneModificationRequiresParentApproval) {
  const session_manager::Session* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  ASSERT_TRUE(active_session);
  const AccountId& account_id = active_session->account_id();
  const user_manager::User* const active_user =
      user_manager::UserManager::Get()->FindUser(account_id);
  ASSERT_TRUE(active_user);
  ASSERT_TRUE(active_user->IsChild());
  Profile* child_profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
  ASSERT_TRUE(child_profile);

  extensions::PrefsUtil prefs_util(child_profile);
  EXPECT_FALSE(ParentAccessService::Get().IsTimezoneChangeApproved(account_id));

  base::Value auto_detect_off(false);
  base::Value resolve_method(3);
  base::Value timezone_1("America/Los_Angeles");

  // Without parent access approval, child user cannot modify timezone prefs.
  EXPECT_EQ(
      prefs_util.SetPref(
          extensions::settings_private::kResolveTimezoneByGeolocationOnOff,
          &auto_detect_off),
      extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
  EXPECT_EQ(prefs_util.SetPref(extensions::settings_private::
                                   kResolveTimezoneByGeolocationMethodShort,
                               &resolve_method),
            extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
  EXPECT_EQ(prefs_util.SetPref(ash::prefs::kUserTimezone, &timezone_1),
            extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);

  // Simulate opening Date & Time settings and completing parent access
  // validation.
  TestDateTimePage test_page;
  mojo::Remote<settings::date_time::mojom::PageHandler> handler_remote;
  auto handler = std::make_unique<settings::NewDateTimeHandler>(
      handler_remote.BindNewPipeAndPassReceiver(), test_page.BindAndGetRemote(),
      /*web_ui=*/nullptr, child_profile);

  handler_remote->ShowParentAccessForTimezone();
  handler_remote.FlushForTesting();
  ASSERT_TRUE(PinRequestWidget::Get());
  PinRequestWidget::Get()->Close(true);
  EXPECT_TRUE(test_page.WaitForParentAccessValidation());
  EXPECT_TRUE(ParentAccessService::Get().IsTimezoneChangeApproved(account_id));

  // After parent access approval, child user can modify timezone prefs.
  EXPECT_EQ(prefs_util.SetPref(extensions::settings_private::
                                   kResolveTimezoneByGeolocationMethodShort,
                               &resolve_method),
            extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(
      prefs_util.SetPref(
          extensions::settings_private::kResolveTimezoneByGeolocationOnOff,
          &auto_detect_off),
      extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(prefs_util.SetPref(ash::prefs::kUserTimezone, &timezone_1),
            extensions::settings_private::SetPrefResult::SUCCESS);

  // Destroying NewDateTimeHandler (closing the Settings page) invalidates the
  // approval.
  handler.reset();
  EXPECT_FALSE(ParentAccessService::Get().IsTimezoneChangeApproved(account_id));

  base::Value auto_detect_on(true);
  base::Value timezone_2("Asia/Tokyo");
  EXPECT_EQ(
      prefs_util.SetPref(
          extensions::settings_private::kResolveTimezoneByGeolocationOnOff,
          &auto_detect_on),
      extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
  EXPECT_EQ(prefs_util.SetPref(ash::prefs::kUserTimezone, &timezone_2),
            extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
}

}  // namespace ash::parent_access
