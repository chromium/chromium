// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_cloud_management_controller_android.h"

#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace policy {

class ChromeBrowserCloudManagementControllerAndroidTest : public testing::Test {
 public:
  ChromeBrowserCloudManagementControllerAndroidTest() = default;
  ~ChromeBrowserCloudManagementControllerAndroidTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest,
       ReadyToCreatePolicyManager) {
  ChromeBrowserCloudManagementControllerAndroid delegate;
  // Not ready to create policy manager when the policy service doesn't exist.
  EXPECT_FALSE(delegate.ReadyToCreatePolicyManager());

  MockPolicyService mock_policy_service;
  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(&mock_policy_service);

  // Not ready to create policy manager before policy service initialization.
  EXPECT_CALL(mock_policy_service,
              IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillOnce(Return(false));
  EXPECT_FALSE(delegate.ReadyToCreatePolicyManager());

  // Ready to create policy manager after policy service initialization.
  EXPECT_CALL(mock_policy_service,
              IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillOnce(Return(true));
  EXPECT_TRUE(delegate.ReadyToCreatePolicyManager());

  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);
}

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest, ReadyToInit) {
  ChromeBrowserCloudManagementControllerAndroid delegate;
  MockPolicyService mock_policy_service;
  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(&mock_policy_service);

  // Not ready to initialize controller before policy service initialization.
  EXPECT_CALL(mock_policy_service,
              IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillOnce(Return(false));
  EXPECT_FALSE(delegate.ReadyToInit());

  // Ready to initialize controller after policy service initialization.
  EXPECT_CALL(mock_policy_service,
              IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillOnce(Return(true));
  EXPECT_TRUE(delegate.ReadyToInit());

  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);
}

TEST_F(ChromeBrowserCloudManagementControllerAndroidTest, DeferInitialization) {
  MockPolicyService mock_policy_service;
  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(&mock_policy_service);

  // Capture the PolicyService observer that will be created by
  // DeferInitialization.
  PolicyService::Observer* captured_observer = nullptr;
  EXPECT_CALL(mock_policy_service, AddObserver(POLICY_DOMAIN_CHROME, _))
      .WillOnce(SaveArg<1>(&captured_observer));

  ChromeBrowserCloudManagementControllerAndroid delegate;
  bool callback_invoked = false;
  delegate.DeferInitialization(
      base::BindOnce([](bool* b) { *b = true; }, &callback_invoked));
  ASSERT_NE(captured_observer, nullptr);
  // The callback passed to DeferInitialization is only expected to be called
  // when PolicyService initialization completes.
  EXPECT_FALSE(callback_invoked);

  // Forces a call to OnPolicyServiceInitialized and expect it to trigger the
  // registered callback invocation.
  captured_observer->OnPolicyServiceInitialized(POLICY_DOMAIN_CHROME);
  EXPECT_TRUE(callback_invoked);
}

}  // namespace policy
