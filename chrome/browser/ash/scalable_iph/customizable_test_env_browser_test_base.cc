// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"

#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/profile_prepared_waiter.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace ash {

namespace {
constexpr char kOwnerEmail[] = "test@example.com";
}

CustomizableTestEnvBrowserTestBase::TestEnvironment::TestEnvironment(
    ash::DeviceStateMixin::State device_state,
    UserSessionType user_session_type)
    : device_state_(device_state), user_session_type_(user_session_type) {}

// static
std::string
CustomizableTestEnvBrowserTestBase::TestEnvironment::GenerateTestName(
    testing::TestParamInfo<CustomizableTestEnvBrowserTestBase::TestEnvironment>
        test_param_info) {
  const CustomizableTestEnvBrowserTestBase::TestEnvironment& test_environment =
      test_param_info.param;

  std::string test_name;
  switch (test_environment.device_state()) {
    case ash::DeviceStateMixin::State::BEFORE_OOBE:
      test_name += "BEFORE_OOBE";
      break;
    case ash::DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED:
      test_name += "OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED";
      break;
    case ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED:
      test_name += "OOBE_COMPLETED_CLOUD_ENROLLED";
      break;
    case ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED:
      test_name += "OOBE_COMPLETED_CONSUMER_OWNED";
      break;
    case ash::DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED:
      test_name += "OOBE_COMPLETED_PERMANENTLY_UNOWNED";
      break;
    case ash::DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED:
      test_name += "OOBE_COMPLETED_UNOWNED";
      break;
    case ash::DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE:
      test_name += "OOBE_COMPLETED_DEMO_MODE";
      break;
  }

  switch (test_environment.user_session_type()) {
    case UserSessionType::kRegular:
      test_name += "_REGULAR";
      break;
    case UserSessionType::kRegularNonOwner:
      test_name += "_REGULAR_NON_OWNER";
      break;
    case UserSessionType::kGuest:
      test_name += "_GUEST";
      break;
    case UserSessionType::kChild:
      test_name += "_CHILD";
      break;
    case UserSessionType::kChildOwner:
      test_name += "_CHILD_OWNER";
      break;
    case UserSessionType::kManaged:
      test_name += "_MANAGED";
      break;
    case UserSessionType::kRegularWithOobe:
      test_name += "_REGULAR_OOBE";
      break;
  }
  return test_name;
}

CustomizableTestEnvBrowserTestBase::CustomizableTestEnvBrowserTestBase()
    : test_environment_(TestEnvironment(
          ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED,
          UserSessionType::kRegular)) {}

CustomizableTestEnvBrowserTestBase::~CustomizableTestEnvBrowserTestBase() =
    default;

void CustomizableTestEnvBrowserTestBase::SetUp() {
  set_up_called_ = true;

  device_state_mixin_ = std::make_unique<ash::DeviceStateMixin>(
      &mixin_host_, test_environment_.device_state());

  switch (test_environment_.user_session_type()) {
    case UserSessionType::kGuest:
      guest_session_mixin_ =
          std::make_unique<ash::GuestSessionMixin>(&mixin_host_);
      break;
    case UserSessionType::kChild:
      logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
          &mixin_host_, ash::LoggedInUserMixin::LogInType::kChild,
          embedded_test_server(), this, /*should_launch_browser=*/false);
      break;
    case UserSessionType::kChildOwner:
      logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
          &mixin_host_, ash::LoggedInUserMixin::LogInType::kChild,
          embedded_test_server(), this, /*should_launch_browser=*/false);
      owner_user_email_ = logged_in_user_mixin_->GetAccountId().GetUserEmail();
      break;
    case UserSessionType::kManaged:
      logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
          &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
          embedded_test_server(), this, /*should_launch_browser=*/false,
          AccountId::FromUserEmailGaiaId(
              FakeGaiaMixin::kEnterpriseUser1,
              FakeGaiaMixin::kEnterpriseUser1GaiaId));

      // If a device is not enrolled, simulate a case where a device is owned by
      // the managed account. This is a managed account on not-enrolled device
      // case. Note that an account (i.e. the managed account) cannot be an
      // owner of a device if a device is enrolled.
      if (test_environment_.device_state() ==
          ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED) {
        owner_user_email_ =
            logged_in_user_mixin_->GetAccountId().GetUserEmail();
      }
      break;
    case UserSessionType::kRegular:
      logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
          &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
          embedded_test_server(), this);
      owner_user_email_ = logged_in_user_mixin_->GetAccountId().GetUserEmail();
      break;
    case UserSessionType::kRegularNonOwner:
      logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
          &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
          embedded_test_server(), this);

      CHECK(kOwnerEmail !=
            logged_in_user_mixin_->GetAccountId().GetUserEmail());
      owner_user_email_ = kOwnerEmail;
      break;
    case UserSessionType::kRegularWithOobe:
      logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
          &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
          embedded_test_server(), this, /*should_launch_browser=*/false,
          /*account_id=*/absl::nullopt, /*include_initial_user=*/false);
      break;
  }

  if (!owner_user_email_.empty()) {
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kDeviceOwner, base::Value(owner_user_email_));
  }

  MixinBasedInProcessBrowserTest::SetUp();
}

void CustomizableTestEnvBrowserTestBase::SetUpOnMainThread() {
  if (logged_in_user_mixin_) {
    if (test_environment_.user_session_type() ==
        UserSessionType::kRegularWithOobe) {
      // For WithOobe session type, we don't wait an active session but a
      // profile creation.
      test::ProfilePreparedWaiter profile_prepared_waiter(
          logged_in_user_mixin_->GetAccountId());
      logged_in_user_mixin_->LogInUser(/*issue_any_scope_token=*/false,
                                       /*wait_for_active_session=*/false,
                                       /*request_policy_update=*/true,
                                       /*skip_post_login_screens=*/false);
      profile_prepared_waiter.Wait();
    } else {
      // `LoggedInUserMixin::LogInUser` waits an active session by default.
      logged_in_user_mixin_->LogInUser();
    }
  }

  MixinBasedInProcessBrowserTest::SetUpOnMainThread();
}

void CustomizableTestEnvBrowserTestBase::SetTestEnvironment(
    const TestEnvironment& test_environment) {
  CHECK(!set_up_called_) << "You are NOT allowed to overwrite test environment "
                            "after the SetUp call.";

  test_environment_ = test_environment;
}

LoginManagerMixin* CustomizableTestEnvBrowserTestBase::GetLoginManagerMixin() {
  CHECK(logged_in_user_mixin_)
      << "LoggedInUserMixin is not set up in the current test environment";
  return logged_in_user_mixin_->GetLoginManagerMixin();
}

}  // namespace ash
