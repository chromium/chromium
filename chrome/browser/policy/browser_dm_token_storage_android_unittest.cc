// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_android.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task_runner_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;

namespace policy {

namespace {

const char kDMToken[] = "fake-dm-token";

}  // namespace

class BrowserDMTokenStorageAndroidTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserDMTokenStorageAndroidTest, InitClientId) {
  BrowserDMTokenStorageAndroid storage;
  EXPECT_THAT(storage.InitClientId(), IsEmpty());
}

TEST_F(BrowserDMTokenStorageAndroidTest, InitEnrollmentToken) {
  BrowserDMTokenStorageAndroid storage;
  EXPECT_THAT(storage.InitEnrollmentToken(), IsEmpty());
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
  base::PostTaskAndReplyWithResult(storage.SaveDMTokenTaskRunner().get(),
                                   FROM_HERE, std::move(task),
                                   std::move(reply));

  run_loop.Run();

  EXPECT_THAT(storage.InitDMToken(), Eq(kDMToken));
}

}  // namespace policy
