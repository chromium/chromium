// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_android.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/android/cloud_management_shared_preferences.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::ReturnRef;

namespace policy {

namespace {

constexpr char kDMToken[] = "fake-dm-token";
constexpr char kEnrollmentToken[] = "fake-enrollment-token";

}  // namespace

class BrowserDMTokenStorageAndroidTest : public testing::Test {
 public:
  void TearDown() override {
    android::SaveDmTokenInSharedPreferences(std::string());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserDMTokenStorageAndroidTest, InitClientId) {
  BrowserDMTokenStorageAndroid storage;
  EXPECT_FALSE(storage.InitClientId().empty());
}

TEST_F(BrowserDMTokenStorageAndroidTest, InitEnrollmentToken) {
  MockPolicyService mock_policy_service;
  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(&mock_policy_service);

  PolicyMap policy_map;
  policy_map.Set(key::kCloudManagementEnrollmentToken, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                 base::Value(kEnrollmentToken), nullptr);
  EXPECT_CALL(mock_policy_service, GetPolicies(_))
      .WillOnce(ReturnRef(policy_map));

  BrowserDMTokenStorageAndroid storage;
  EXPECT_THAT(storage.InitEnrollmentToken(), Eq(kEnrollmentToken));

  BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);
}

TEST_F(BrowserDMTokenStorageAndroidTest, InitDMToken) {
  BrowserDMTokenStorageAndroid storage;
  EXPECT_THAT(storage.InitDMToken(), IsEmpty());
}

TEST_F(BrowserDMTokenStorageAndroidTest, InitEnrollmentErrorOption) {
  BrowserDMTokenStorageAndroid storage;
  EXPECT_FALSE(storage.InitEnrollmentErrorOption());
}

class TestStoreDMTokenDelegate {
 public:
  MOCK_METHOD(void, OnDMTokenStored, (bool success));
};

TEST_F(BrowserDMTokenStorageAndroidTest, SaveDMToken) {
  TestStoreDMTokenDelegate callback_delegate;

  base::RunLoop run_loop;
  EXPECT_CALL(callback_delegate, OnDMTokenStored(true))
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));

  BrowserDMTokenStorageAndroid storage;
  auto task = storage.SaveDMTokenTask(kDMToken, storage.InitClientId());
  auto reply = base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenStored,
                              base::Unretained(&callback_delegate));
  storage.SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(task), std::move(reply));

  run_loop.Run();

  EXPECT_THAT(storage.InitDMToken(), Eq(kDMToken));
}

}  // namespace policy
