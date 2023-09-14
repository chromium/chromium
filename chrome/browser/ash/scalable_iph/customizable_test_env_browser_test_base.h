// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCALABLE_IPH_CUSTOMIZABLE_TEST_ENV_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_ASH_SCALABLE_IPH_CUSTOMIZABLE_TEST_ENV_BROWSER_TEST_BASE_H_

#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CustomizableTestEnvBrowserTestBase
    : public MixinBasedInProcessBrowserTest {
 public:
  enum class UserSessionType {
    kGuest,
    kChild,
    kChildOwner,
    kRegular,
    kRegularNonOwner,
    kManaged,
    kRegularWithOobe
  };

  class TestEnvironment {
   public:
    TestEnvironment(ash::DeviceStateMixin::State device_state,
                    UserSessionType user_session_type);

    static std::string GenerateTestName(
        testing::TestParamInfo<
            CustomizableTestEnvBrowserTestBase::TestEnvironment>
            test_param_info);

    ash::DeviceStateMixin::State device_state() const { return device_state_; }
    UserSessionType user_session_type() const { return user_session_type_; }

   private:
    ash::DeviceStateMixin::State device_state_;
    UserSessionType user_session_type_;
  };

  CustomizableTestEnvBrowserTestBase();
  ~CustomizableTestEnvBrowserTestBase() override;

  // MixinBasedInProcessBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;

 protected:
  void SetTestEnvironment(const TestEnvironment& test_environment);
  LoginManagerMixin* GetLoginManagerMixin();
  const TestEnvironment& test_environment() const { return test_environment_; }

 private:
  std::string owner_user_email_;
  std::unique_ptr<ash::GuestSessionMixin> guest_session_mixin_;
  std::unique_ptr<ash::LoggedInUserMixin> logged_in_user_mixin_;
  std::unique_ptr<ash::DeviceStateMixin> device_state_mixin_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  // This is initialized to a regular account on a consumer owned device. A
  // subclass can overwrite this by calling `SetTestEnvironment` before `SetUp`
  // call. Note that it means that an extended class can overwrite this at any
  // time before `SetUp` call. Do NOT rely on a value before `SetUp` call.
  TestEnvironment test_environment_;
  bool set_up_called_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCALABLE_IPH_CUSTOMIZABLE_TEST_ENV_BROWSER_TEST_BASE_H_
