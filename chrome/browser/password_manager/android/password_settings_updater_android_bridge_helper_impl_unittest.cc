// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::Eq;
using testing::Mock;
using testing::StrictMock;
using SyncingAccount =
    PasswordSettingsUpdaterAndroidDispatcherBridge::SyncingAccount;

constexpr char kTestAccount[] = "test@gmail.com";

class MockConsumer
    : public PasswordSettingsUpdaterAndroidReceiverBridge::Consumer {
  MOCK_METHOD(void,
              OnSettingValueFetched,
              (PasswordManagerSetting, bool),
              (override));
  MOCK_METHOD(void, OnSettingValueAbsent, (PasswordManagerSetting), (override));
  MOCK_METHOD(void,
              OnSettingFetchingError,
              (PasswordManagerSetting, AndroidBackendAPIErrorCode),
              (override));
  MOCK_METHOD(void,
              OnSuccessfulSettingChange,
              (PasswordManagerSetting),
              (override));
  MOCK_METHOD(void,
              OnFailedSettingChange,
              (PasswordManagerSetting, AndroidBackendAPIErrorCode),
              (override));
};

class MockPasswordSettingsUpdaterAndroidReceiverBridge
    : public PasswordSettingsUpdaterAndroidReceiverBridge {
 public:
  MOCK_METHOD(
      void,
      SetConsumer,
      (base::WeakPtr<PasswordSettingsUpdaterAndroidReceiverBridge::Consumer>),
      (override));
  MOCK_METHOD(base::android::ScopedJavaGlobalRef<jobject>,
              GetJavaBridge,
              (),
              (const, override));
};

class MockPasswordSettingsUpdaterAndroidDispatcherBridge
    : public PasswordSettingsUpdaterAndroidDispatcherBridge {
 public:
  MOCK_METHOD(void,
              Init,
              (base::android::ScopedJavaGlobalRef<jobject>),
              (override));
  MOCK_METHOD(void,
              GetPasswordSettingValue,
              (std::optional<SyncingAccount>, PasswordManagerSetting),
              (override));
  MOCK_METHOD(void,
              SetPasswordSettingValue,
              (std::optional<SyncingAccount>, PasswordManagerSetting, bool),
              (override));
};

}  // namespace

class PasswordSettingsUpdaterAndroidBridgeHelperImplTest
    : public testing::Test {
 protected:
  PasswordSettingsUpdaterAndroidBridgeHelperImplTest()
      : helper_(base::PassKey<
                    class PasswordSettingsUpdaterAndroidBridgeHelperImplTest>(),
                CreateMockReceiverBridge(),
                CreateMockDispatcherBridge()) {
    helper_.SetConsumer(consumer_weak_factory_.GetWeakPtr());
    RunUntilIdle();
  }

  ~PasswordSettingsUpdaterAndroidBridgeHelperImplTest() override {
    RunUntilIdle();
    Mock::VerifyAndClearExpectations(receiver_bridge_);
    Mock::VerifyAndClearExpectations(dispatcher_bridge_);
    Mock::VerifyAndClearExpectations(&consumer_);
  }

  MockPasswordSettingsUpdaterAndroidReceiverBridge* receiver_bridge() {
    return receiver_bridge_;
  }
  MockPasswordSettingsUpdaterAndroidDispatcherBridge* dispatcher_bridge() {
    return dispatcher_bridge_;
  }
  PasswordSettingsUpdaterAndroidBridgeHelperImpl* helper() { return &helper_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

 private:
  std::unique_ptr<MockPasswordSettingsUpdaterAndroidReceiverBridge>
  CreateMockReceiverBridge() {
    auto unique_receiver_bridge = std::make_unique<
        StrictMock<MockPasswordSettingsUpdaterAndroidReceiverBridge>>();
    receiver_bridge_ = unique_receiver_bridge.get();

    EXPECT_CALL(*receiver_bridge(), GetJavaBridge);
    EXPECT_CALL(*receiver_bridge(), SetConsumer);

    return unique_receiver_bridge;
  }

  std::unique_ptr<PasswordSettingsUpdaterAndroidDispatcherBridge>
  CreateMockDispatcherBridge() {
    auto unique_dispatcher_bridge = std::make_unique<
        StrictMock<MockPasswordSettingsUpdaterAndroidDispatcherBridge>>();
    dispatcher_bridge_ = unique_dispatcher_bridge.get();

    EXPECT_CALL(*dispatcher_bridge(), Init);

    return unique_dispatcher_bridge;
  }

  raw_ptr<StrictMock<MockPasswordSettingsUpdaterAndroidReceiverBridge>>
      receiver_bridge_;
  raw_ptr<StrictMock<MockPasswordSettingsUpdaterAndroidDispatcherBridge>>
      dispatcher_bridge_;
  PasswordSettingsUpdaterAndroidBridgeHelperImpl helper_;
  StrictMock<MockConsumer> consumer_;
  base::WeakPtrFactory<MockConsumer> consumer_weak_factory_{&consumer_};
};

TEST_F(PasswordSettingsUpdaterAndroidBridgeHelperImplTest,
       GetPasswordSettingValueCallsBridge) {
  helper()->GetPasswordSettingValue(
      SyncingAccount(kTestAccount),
      PasswordManagerSetting::kOfferToSavePasswords);
  EXPECT_CALL(*dispatcher_bridge(),
              GetPasswordSettingValue(
                  Eq(SyncingAccount(kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords)));
  RunUntilIdle();
}

TEST_F(PasswordSettingsUpdaterAndroidBridgeHelperImplTest,
       SetPasswordSettingValueCallsBridge) {
  helper()->SetPasswordSettingValue(
      SyncingAccount(kTestAccount),
      PasswordManagerSetting::kOfferToSavePasswords, true);
  EXPECT_CALL(*dispatcher_bridge(),
              SetPasswordSettingValue(
                  Eq(SyncingAccount(kTestAccount)),
                  Eq(PasswordManagerSetting::kOfferToSavePasswords), Eq(true)));
  RunUntilIdle();
}

}  // namespace password_manager
