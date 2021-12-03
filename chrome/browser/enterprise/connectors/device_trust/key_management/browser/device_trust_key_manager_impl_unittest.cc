// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include "base/barrier_closure.h"
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace enterprise_connectors {

using test::MockKeyRotationLauncher;

namespace {

constexpr char kFakeData[] = "some fake string";

constexpr char kLoadedKeyTrustLevelHistogram[] =
    "Enterprise.DeviceTrust.Key.TrustLevel";
constexpr char kLoadedKeyTypeHistogram[] = "Enterprise.DeviceTrust.Key.Type";
constexpr char kKeyCreationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.CreationResult";
constexpr char kKeyRotationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.RotationResult";

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

  void ExpectLoadedTpmKeyMetrics() {
    // A TPM-generated key was successfully loaded. We don't know which
    // algorithm was used though, so just check that it was logged only once.
    histogram_tester_.ExpectUniqueSample(kLoadedKeyTrustLevelHistogram,
                                         DTKeyTrustLevel::kTpm, 1);
    histogram_tester_.ExpectTotalCount(kLoadedKeyTypeHistogram, 1);
  }

  void ExpectKeyCreatedMetrics() {
    histogram_tester_.ExpectUniqueSample(kKeyCreationResultHistogram,
                                         DTKeyRotationResult::kSucceeded, 1);
    histogram_tester_.ExpectTotalCount(kKeyRotationResultHistogram, 0);
  }

  DeviceTrustKeyManagerImpl* key_manager() { return key_manager_.get(); }
  StrictMock<MockKeyRotationLauncher>* mock_launcher() {
    return mock_launcher_;
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
  raw_ptr<StrictMock<MockKeyRotationLauncher>> mock_launcher_;

  std::unique_ptr<DeviceTrustKeyManagerImpl> key_manager_;
};

// Tests that StartInitialization will load a key and not trigger key creation
// if key loading was successful.
TEST_F(DeviceTrustKeyManagerImplTest, Initialization_WithPersistedKey) {
  SetUpPersistedKey();

  key_manager()->StartInitialization();

  base::RunLoop run_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&run_loop](absl::optional<std::string> value) {
        EXPECT_TRUE(value);
        EXPECT_FALSE(value->empty());
        run_loop.Quit();
      }));

  run_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  histogram_tester_.ExpectTotalCount(kKeyCreationResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kKeyRotationResultHistogram, 0);
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful.
// - Key creation succeeds and a key gets loaded successfully,
// - Then a client request gets replied successfully.
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_CreatesKey_LoadKeySuccess) {
  SetUpNoKey();

  base::RunLoop create_key_loop;
  KeyRotationCommand::Callback key_rotation_callback;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            key_rotation_callback = std::move(callback);
            create_key_loop.Quit();
          }));

  key_manager()->StartInitialization();

  create_key_loop.Run();

  // Mimic that the key is now loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  // The manager should now respond to the callback as soon as the key is
  // loaded.
  base::RunLoop run_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&run_loop](absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  ExpectKeyCreatedMetrics();
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful.
// - Key creation succeeds, but the subsequent key loading fails.
// - Then a client request makes the key loading retry, but fail.
//   - But no key creation happens again
// - Then a second client request makes the key load successfully.
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_CreatesKey_LoadKeyFail_Retry) {
  SetUpNoKey();

  KeyRotationCommand::Callback key_rotation_callback;
  base::RunLoop create_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            key_rotation_callback = std::move(callback);
            create_key_loop.Quit();
          }));

  key_manager()->StartInitialization();

  create_key_loop.Run();

  // Mimic that the key creation was successful, however set key loading mocks
  // to mimic a loading failure.
  SetUpNoKey();
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  // The manager should now try to load the key upon receiving the client
  // request, but will fail to do so. It will then reply with a failure to the
  // client requests.
  base::RunLoop fail_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&fail_loop](absl::optional<std::string> value) {
        EXPECT_FALSE(value);
        fail_loop.Quit();
      }));

  fail_loop.Run();

  // Retry, but with a successful key loading this time.
  SetUpPersistedKey();

  base::RunLoop success_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&success_loop](absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        success_loop.Quit();
      }));

  success_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  ExpectKeyCreatedMetrics();
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful.
// - Key creation fails,
// - Subsequent client requests will retry key creation,
// - Key creation then succeeds,
// - Key loading succeeds,
// - The client request gets fulfilled.
TEST_F(DeviceTrustKeyManagerImplTest, Initialization_CreateFails_Retry) {
  SetUpNoKey();

  KeyRotationCommand::Callback failed_rotation_callback;
  base::RunLoop create_key_fail_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            failed_rotation_callback = std::move(callback);
            create_key_fail_loop.Quit();
          }));

  key_manager()->StartInitialization();

  create_key_fail_loop.Run();

  // Mimic that key creation failed.
  SetUpNoKey();
  ASSERT_FALSE(failed_rotation_callback.is_null());
  std::move(failed_rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(kKeyCreationResultHistogram,
                                       DTKeyRotationResult::kFailed, 1);

  KeyRotationCommand::Callback success_rotation_callback;
  base::RunLoop create_key_success_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            success_rotation_callback = std::move(callback);
            create_key_success_loop.Quit();
          }));

  // This client request will try to load the key, then fail (since key creation
  // failed previously), and then trigger a successful key creation followed
  // by a successful key loading.
  base::RunLoop request_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&request_loop](absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        request_loop.Quit();
      }));

  create_key_success_loop.Run();

  // Make the key creation return a successful status and fake that a key is
  // loadable.
  SetUpPersistedKey();
  ASSERT_FALSE(success_rotation_callback.is_null());
  std::move(success_rotation_callback)
      .Run(KeyRotationCommand::Status::SUCCEEDED);

  // The client request should be responded to.
  request_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  histogram_tester_.ExpectTotalCount(kKeyRotationResultHistogram, 0);
  histogram_tester_.ExpectBucketCount(kKeyCreationResultHistogram,
                                      DTKeyRotationResult::kSucceeded, 1);
}

// Tests a long and specific chain of events which are, in sequence:
// - Key Manager initialization started,
// - Key loading starts,
// - Client requests (batch 1) come in and are pending,
// - Key loading fails,
// - Key creation process is launched,
// - New client requests (batch 2) come in and are pending,
// - Key creation process succeeds.
// - Key loading starts,
// - New client requests (batch 3) come in and are pending,
// - Key loading succeeds.
//   - All pending requests (batches 1, 2 and 3) are successfully answered.
// <end of test>
// This test also covers both client APIs (ExportPublicKeyAsync and
// SignStringAsync).
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_CreatesKey_SubsequentConcurrentCalls) {
  SetUpNoKey();

  KeyRotationCommand::Callback key_rotation_callback;
  base::RunLoop create_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            key_rotation_callback = std::move(callback);
            create_key_loop.Quit();
          }));

  key_manager()->StartInitialization();

  // A total of 6 callbacks will be marked as pending during this whole test.
  base::RunLoop barrier_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(6, barrier_loop.QuitClosure());

  auto export_key_counter = 0;
  auto export_key_callback =
      base::BindLambdaForTesting([&export_key_counter, &barrier_closure](
                                     absl::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        ++export_key_counter;
        barrier_closure.Run();
      });

  auto sign_string_counter = 0;
  auto sign_string_callback = base::BindLambdaForTesting(
      [&sign_string_counter,
       &barrier_closure](absl::optional<std::vector<uint8_t>> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        ++sign_string_counter;
        barrier_closure.Run();
      });

  // These initial requests should be queued-up since the key is currently being
  // created.
  key_manager()->ExportPublicKeyAsync(export_key_callback);
  key_manager()->SignStringAsync(kFakeData, sign_string_callback);

  create_key_loop.Run();

  // Key creation should not be triggered again.
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(_, _)).Times(0);

  // Queue-up more requests, which should also be set as pending since key
  // creation is still running.
  key_manager()->ExportPublicKeyAsync(export_key_callback);
  key_manager()->SignStringAsync(kFakeData, sign_string_callback);

  // Prepare for another key load, but with a valid key this time.
  SetUpPersistedKey();
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // Queue-up more requests, which should be executed normally.
  key_manager()->ExportPublicKeyAsync(export_key_callback);
  key_manager()->SignStringAsync(kFakeData, sign_string_callback);

  // All pending callbacks should get called now.
  barrier_loop.Run();

  ExpectLoadedTpmKeyMetrics();
  ExpectKeyCreatedMetrics();
}

}  // namespace enterprise_connectors
