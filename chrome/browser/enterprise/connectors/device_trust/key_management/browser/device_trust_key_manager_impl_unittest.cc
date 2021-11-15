// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::StrictMock;

namespace enterprise_connectors {

using test::MockKeyRotationLauncher;

namespace {

enterprise_connectors::test::MockKeyPersistenceDelegate::KeyInfo
CreateEmptyKey() {
  return {enterprise_management::BrowserPublicKeyUploadRequest::
              KEY_TRUST_LEVEL_UNSPECIFIED,
          std::vector<uint8_t>()};
}

}  // namespace

class DeviceTrustKeyManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    auto mock_launcher =
        std::make_unique<StrictMock<MockKeyRotationLauncher>>();
    mock_launcher_ = mock_launcher.get();

    key_manager_ =
        std::make_unique<DeviceTrustKeyManagerImpl>(std::move(mock_launcher));
  }

  void SetUpPersistedKey() {
    // ScopedKeyPersistenceDelegateFactory creates mocked persistence delegates
    // that already mimic the existence of a TPM key provider and stored key.
    auto mock_persistence_delegate =
        persistence_delegate_factory_.CreateMockedTpmDelegate();
    EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
    EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());

    persistence_delegate_factory_.set_next_instance(
        std::move(mock_persistence_delegate));
  }

  void SetUpNoKey() {
    auto mock_persistence_delegate =
        std::make_unique<test::MockKeyPersistenceDelegate>();
    EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
        .WillOnce(testing::Return(CreateEmptyKey()));

    persistence_delegate_factory_.set_next_instance(
        std::move(mock_persistence_delegate));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  DeviceTrustKeyManagerImpl* key_manager() { return key_manager_.get(); }
  StrictMock<MockKeyRotationLauncher>* mock_launcher() {
    return mock_launcher_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
  StrictMock<MockKeyRotationLauncher>* mock_launcher_;

  std::unique_ptr<DeviceTrustKeyManagerImpl> key_manager_;
};

// Tests that StartInitialization will load a key and not trigger key creation
// if key loading was successful.
TEST_F(DeviceTrustKeyManagerImplTest, Initialization_WithPersistedKey) {
  SetUpPersistedKey();

  key_manager()->StartInitialization();

  absl::optional<std::string> captured_str;
  bool callback_called;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&captured_str, &callback_called](absl::optional<std::string> value) {
        captured_str = value;
        callback_called = true;
      }));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(captured_str.has_value());

  // Reset.
  callback_called = false;

  RunUntilIdle();

  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&captured_str, &callback_called](absl::optional<std::string> value) {
        captured_str = value;
        callback_called = true;
      }));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(captured_str.has_value());
}

// Tests that StartInitialization will trigger key creation if key loading was
// not successful.
TEST_F(DeviceTrustKeyManagerImplTest, Initialization_WithoutPersistedKey) {
  SetUpNoKey();

  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string()))
      .WillOnce(testing::Return(true));

  key_manager()->StartInitialization();
  RunUntilIdle();

  // Key creation will have been launched, but no key is yet loaded.
  absl::optional<std::string> captured_str;
  bool callback_called;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&captured_str, &callback_called](absl::optional<std::string> value) {
        captured_str = value;
        callback_called = true;
      }));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(captured_str.has_value());
}

}  // namespace enterprise_connectors
