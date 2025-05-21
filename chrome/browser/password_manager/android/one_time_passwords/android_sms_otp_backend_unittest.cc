// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_backend.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Return;
using testing::StrictMock;

class MockAndroidSmsOtpFetchReceiverBridge
    : public AndroidSmsOtpFetchReceiverBridge {
 public:
  MOCK_METHOD(base::android::ScopedJavaGlobalRef<jobject>,
              GetJavaBridge,
              (),
              (const));
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), ());
};

class MockAndroidSmsOtpFetchDispatcherBridge
    : public AndroidSmsOtpFetchDispatcherBridge {
 public:
  MOCK_METHOD(bool, Init, (base::android::ScopedJavaGlobalRef<jobject>), ());
  MOCK_METHOD(void, RetrieveSmsOtp, (), ());
};

}  // namespace

class AndroidSmsOtpBackendTest : public testing::Test {
 protected:
  AndroidSmsOtpBackend CreateBackend(
      std::unique_ptr<AndroidSmsOtpFetchReceiverBridge> receiver_bridge,
      std::unique_ptr<AndroidSmsOtpFetchDispatcherBridge> dispatcher_bridge,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner) {
    EXPECT_CALL(*receiver_bridge_, GetJavaBridge);
    EXPECT_CALL(*receiver_bridge_, SetConsumer);
    return AndroidSmsOtpBackend(base::PassKey<class AndroidSmsOtpBackendTest>(),
                                std::move(receiver_bridge),
                                std::move(dispatcher_bridge),
                                background_task_runner);
  }

  std::unique_ptr<AndroidSmsOtpFetchReceiverBridge> CreateMockReceiverBridge() {
    auto unique_receiver_bridge =
        std::make_unique<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>>();
    receiver_bridge_ = unique_receiver_bridge.get();
    return unique_receiver_bridge;
  }

  std::unique_ptr<AndroidSmsOtpFetchDispatcherBridge>
  CreateMockDispatcherBridge() {
    auto unique_dispatcher_bridge =
        std::make_unique<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>();
    dispatcher_bridge_ = unique_dispatcher_bridge.get();
    return unique_dispatcher_bridge;
  }

  raw_ptr<StrictMock<MockAndroidSmsOtpFetchReceiverBridge>> receiver_bridge_;
  raw_ptr<StrictMock<MockAndroidSmsOtpFetchDispatcherBridge>>
      dispatcher_bridge_;
  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(AndroidSmsOtpBackendTest, BackendInitFails) {
  AndroidSmsOtpBackend backend =
      CreateBackend(CreateMockReceiverBridge(), CreateMockDispatcherBridge(),
                    background_task_runner_);

  // Run tasks on the background thread to trigger calls to the dispatcher
  // bridge.
  EXPECT_CALL(*dispatcher_bridge_, Init).WillOnce(Return(false));
  background_task_runner_->RunPendingTasks();

  // No fetch requests should be made if initialization failed.
  EXPECT_CALL(*dispatcher_bridge_, RetrieveSmsOtp).Times(0);
  backend.RetrieveSmsOtp();
}

TEST_F(AndroidSmsOtpBackendTest, BackendInitSucceeds) {
  AndroidSmsOtpBackend backend =
      CreateBackend(CreateMockReceiverBridge(), CreateMockDispatcherBridge(),
                    background_task_runner_);

  // Run tasks on the background thread to trigger calls to the dispatcher
  // bridge.
  EXPECT_CALL(*dispatcher_bridge_, Init).WillOnce(Return(true));
  background_task_runner_->RunPendingTasks();

  EXPECT_CALL(*dispatcher_bridge_, RetrieveSmsOtp);
  backend.RetrieveSmsOtp();
}

TEST_F(AndroidSmsOtpBackendTest,
       FetchRequestReceivedBeforeBackendInitComplete) {
  AndroidSmsOtpBackend backend =
      CreateBackend(CreateMockReceiverBridge(), CreateMockDispatcherBridge(),
                    background_task_runner_);

  // No fetching should happen before initialization is complete.
  EXPECT_CALL(*dispatcher_bridge_, RetrieveSmsOtp).Times(0);
  backend.RetrieveSmsOtp();

  // Fetch request should happen once initialization is complete.
  EXPECT_CALL(*dispatcher_bridge_, Init).WillOnce(Return(true));
  EXPECT_CALL(*dispatcher_bridge_, RetrieveSmsOtp);
  background_task_runner_->RunPendingTasks();
}
