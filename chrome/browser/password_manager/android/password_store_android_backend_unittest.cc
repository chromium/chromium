// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::Return;
using testing::StrictMock;
using TaskId = PasswordStoreAndroidBackendBridge::TaskId;

std::vector<std::unique_ptr<PasswordForm>> CreateTestLogins() {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              /*psl=*/false,
                              /*affiliation=*/false));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"), /*psl=*/true,
                              /*affiliation=*/false));
  return forms;
}

std::vector<PasswordForm> UnwrapForms(
    std::vector<std::unique_ptr<PasswordForm>> password_ptrs) {
  std::vector<PasswordForm> forms;
  forms.reserve(password_ptrs.size());
  for (auto& password : password_ptrs) {
    forms.push_back(std::move(*password));
  }
  return forms;
}

class MockPasswordStoreAndroidBackendBridge
    : public PasswordStoreAndroidBackendBridge {
 public:
  MOCK_METHOD(void, SetConsumer, (Consumer*), (override));
  MOCK_METHOD(TaskId, GetAllLogins, (), (override));
};

}  // namespace

class PasswordStoreAndroidBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendTest() {
    backend_ =
        std::make_unique<PasswordStoreAndroidBackend>(CreateMockBridge());
  }

  ~PasswordStoreAndroidBackendTest() override {
    testing::Mock::VerifyAndClearExpectations(bridge_);
    EXPECT_CALL(*bridge_, SetConsumer(nullptr));
  }

  PasswordStoreBackend& backend() { return *backend_; }
  PasswordStoreAndroidBackendBridge::Consumer& consumer() { return *backend_; }
  MockPasswordStoreAndroidBackendBridge* bridge() { return bridge_; }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  std::unique_ptr<PasswordStoreAndroidBackendBridge> CreateMockBridge() {
    auto unique_bridge =
        std::make_unique<StrictMock<MockPasswordStoreAndroidBackendBridge>>();
    bridge_ = unique_bridge.get();
    EXPECT_CALL(*bridge_, SetConsumer);
    return unique_bridge;
  }
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<PasswordStoreAndroidBackend> backend_;
  StrictMock<MockPasswordStoreAndroidBackendBridge>* bridge_;
};

TEST_F(PasswordStoreAndroidBackendTest, CallsCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true));
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), completion_callback.Get());
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForLogins) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const TaskId kTaskId{1337};
  base::MockCallback<LoginsReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kTaskId));
  backend().GetAllLoginsAsync(mock_reply.Get());

  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kTaskId, UnwrapForms(CreateTestLogins()));
  RunUntilIdle();
}

}  // namespace password_manager
