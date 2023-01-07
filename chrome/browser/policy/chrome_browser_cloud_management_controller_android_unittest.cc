// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller_android.h"

#include "chrome/browser/policy/android/cloud_management_shared_preferences.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace policy {

class ChromeBrowserCloudManagementControllerAndroidTest : public testing::Test {
 public:
  ChromeBrowserCloudManagementControllerAndroidTest() = default;
  ~ChromeBrowserCloudManagementControllerAndroidTest() override = default;

  void SetUp() override {
    BrowserPolicyConnectorBase::SetPolicyServiceForTesting(
        &mock_policy_service_);
  }

  void TearDown() override {
    BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);
    android::SaveDmTokenInSharedPreferences(std::string());
  }

 protected:
  MockPolicyService mock_policy_service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest,
       ReadyToCreatePolicyManager_DMToken) {
  PolicyMap policy_map;
  EXPECT_CALL(mock_policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policy_map));

  ChromeBrowserCloudManagementControllerAndroid delegate;
  // Not ready to create policy manager when the DMToken is empty and the
  // enrollment token is not available in the policy service.
  EXPECT_FALSE(delegate.ReadyToCreatePolicyManager());

  // Ready to create policy manager because the DMToken is set.
  android::SaveDmTokenInSharedPreferences("dm-token");
  EXPECT_TRUE(delegate.ReadyToCreatePolicyManager());
}

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest,
       ReadyToCreatePolicyManager_EnrollmentToken) {
  PolicyMap policy_map;
  EXPECT_CALL(mock_policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policy_map));

  ChromeBrowserCloudManagementControllerAndroid delegate;
  // Not ready to create policy manager when the DMToken is empty and the
  // enrollment token is not available in the policy service.
  EXPECT_FALSE(delegate.ReadyToCreatePolicyManager());

  // Ready to create policy manager after enrollment token available in the
  // policy service.
  policy_map.Set(key::kCloudManagementEnrollmentToken, PolicyMap::Entry());
  EXPECT_TRUE(delegate.ReadyToCreatePolicyManager());
}

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest, ReadyToInit_DMToken) {
  PolicyMap policy_map;
  EXPECT_CALL(mock_policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policy_map));

  ChromeBrowserCloudManagementControllerAndroid delegate;
  // Not ready to initialize controller when the DMToken is empty and the
  // enrollment token is not available in the policy service.
  EXPECT_FALSE(delegate.ReadyToInit());

  // Ready to initialize controller because DMToken is available.
  android::SaveDmTokenInSharedPreferences("dm-token");
  EXPECT_TRUE(delegate.ReadyToInit());
}

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest,
       ReadyToInit_EnrollmentToken) {
  PolicyMap policy_map;
  EXPECT_CALL(mock_policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policy_map));

  ChromeBrowserCloudManagementControllerAndroid delegate;
  // Not ready to initialize controller when the DMToken is empty and the
  // enrollment token is not available in the policy service.
  EXPECT_FALSE(delegate.ReadyToInit());

  // Not ready to initialize controller after enrollment token available in the
  // policy service.
  policy_map.Set(key::kCloudManagementEnrollmentToken, PolicyMap::Entry());
  EXPECT_TRUE(delegate.ReadyToInit());
}

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest, DeferInitialization) {
  // Capture the ProviderUpdateObserver that will be created by
  // DeferInitialization.
  PolicyMap policy_map;
  EXPECT_CALL(mock_policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policy_map));
  PolicyService::ProviderUpdateObserver* captured_observer = nullptr;
  EXPECT_CALL(mock_policy_service_, AddProviderUpdateObserver(_))
      .WillOnce(SaveArg<0>(&captured_observer));

  ChromeBrowserCloudManagementControllerAndroid delegate;
  bool callback_invoked = false;
  delegate.DeferInitialization(
      base::BindOnce([](bool* b) { *b = true; }, &callback_invoked));
  ASSERT_NE(captured_observer, nullptr);
  // The callback passed to DeferInitialization is only expected to be called
  // when the platform provider produces an enrollment token.
  EXPECT_FALSE(callback_invoked);

  // Forces a call to OnProviderUpdatePropagated with a policy provider
  // different from the platform provider and expect it to trigger the
  // registered callback invocation.
  captured_observer->OnProviderUpdatePropagated(
      g_browser_process->browser_policy_connector()
          ->command_line_policy_provider());
  EXPECT_FALSE(callback_invoked);

  // Forces a call to OnProviderUpdatePropagated with the platform policy
  // provider. Expect it to trigger the registered callback invocation only
  // after the enrollment token policy is set.
  captured_observer->OnProviderUpdatePropagated(
      g_browser_process->browser_policy_connector()->GetPlatformProvider());
  EXPECT_FALSE(callback_invoked);

  // Forces a call to OnProviderUpdatePropagated with the platform policy
  // provider and expect it to trigger the registered callback invocation.
  policy_map.Set(key::kCloudManagementEnrollmentToken, PolicyMap::Entry());
  captured_observer->OnProviderUpdatePropagated(
      g_browser_process->browser_policy_connector()->GetPlatformProvider());
  EXPECT_TRUE(callback_invoked);
}

}  // namespace policy
