// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <memory>

#include "base/callback_forward.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::Return;
using testing::StrictMock;
using TaskId = PasswordStoreAndroidBackendBridge::TaskId;

class MockPasswordStoreAndroidBackendBridge
    : public PasswordStoreAndroidBackendBridge {
 public:
  MOCK_METHOD(TaskId, GetAllLogins, (), (override));
};

}  // namespace

class PasswordStoreAndroidBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendTest() {
    backend_ =
        std::make_unique<PasswordStoreAndroidBackend>(CreateMockBridge());
  }

  PasswordStoreBackend& backend() { return *backend_; }
  MockPasswordStoreAndroidBackendBridge* bridge() { return bridge_; }

 private:
  std::unique_ptr<PasswordStoreAndroidBackendBridge> CreateMockBridge() {
    auto unique_bridge =
        std::make_unique<StrictMock<MockPasswordStoreAndroidBackendBridge>>();
    bridge_ = unique_bridge.get();
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
  // TODO(https://crbug.com/1229654): Expect correct LoginsReply to be called.
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(TaskId(1337)));
  backend().GetAllLoginsAsync(LoginsReply());
}

}  // namespace password_manager
