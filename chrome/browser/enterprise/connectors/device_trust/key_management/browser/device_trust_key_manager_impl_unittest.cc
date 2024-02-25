// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"

#include <array>
#include <optional>

#include "base/barrier_closure.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_key_loader.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace enterprise_connectors {

using test::MockKeyLoader;
using test::MockKeyRotationLauncher;
using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using DTCLoadKeyResult = KeyLoader::DTCLoadKeyResult;
using RotateKeyCallback = DeviceTrustKeyManagerImpl::RotateKeyCallback;
using KeyRotationResult = DeviceTrustKeyManager::KeyRotationResult;
using PermanentFailure = DeviceTrustKeyManager::PermanentFailure;

namespace {

constexpr int kSuccessUploadCode = 200;

constexpr char kFakeNonce[] = "fake nonce";
constexpr char kOtherFakeNonce[] = "other fake nonce";
constexpr char kFakeData[] = "some fake string";

constexpr char kLoadedKeyTrustLevelHistogram[] =
    "Enterprise.DeviceTrust.Key.TrustLevel";
constexpr char kLoadedKeyTypeHistogram[] = "Enterprise.DeviceTrust.Key.Type";
constexpr char kKeyCreationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.CreationResult";
constexpr char kKeyRotationResultHistogram[] =
    "Enterprise.DeviceTrust.Key.RotationResult";

scoped_refptr<SigningKeyPair> CreateFakeHWKeyPair() {
  ECSigningKeyProvider provider;
  auto algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider.GenerateSigningKeySlowly(algorithm);
  DCHECK(signing_key);
  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              BPKUR::CHROME_BROWSER_HW_KEY);
}

}  // namespace

class DeviceTrustKeyManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    test_key_pair_ = CreateFakeHWKeyPair();
    ResetState();
  }

  scoped_refptr<SigningKeyPair> test_key_pair() { return test_key_pair_; }

  void SetUpKeyLoadAndSyncWithSideEffect(base::RepeatingClosure& side_effect) {
    EXPECT_CALL(*mock_loader_, LoadKey(_))
        .WillRepeatedly(
            Invoke([side_effect, this](KeyLoader::LoadKeyCallback callback) {
              side_effect.Run();
              std::move(callback).Run(
                  DTCLoadKeyResult(kSuccessUploadCode, test_key_pair_));
            }));
  }

  void SetUpKeyLoadAndSyncWithSideEffect(
      const DTCLoadKeyResult& load_key_result,
      base::RepeatingClosure& side_effect) {
    EXPECT_CALL(*mock_loader_, LoadKey(_))
        .WillRepeatedly(Invoke([side_effect, load_key_result](
                                   KeyLoader::LoadKeyCallback callback) {
          side_effect.Run();
          std::move(callback).Run(load_key_result);
        }));
  }

  void SetUpKeyLoadAndSync(const DTCLoadKeyResult& load_key_result) {
    EXPECT_CALL(*mock_loader_, LoadKey(_))
        .WillRepeatedly(
            Invoke([load_key_result](KeyLoader::LoadKeyCallback callback) {
              std::move(callback).Run(load_key_result);
            }));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExpectLoadedHardwareKeyMetrics(int times_loaded = 1) {
    // A hardware-generated key was successfully loaded. We don't know which
    // algorithm was used though, so just check that it was logged only once.
    histogram_tester_->ExpectUniqueSample(kLoadedKeyTrustLevelHistogram,
                                          DTKeyTrustLevel::kHw, times_loaded);
    histogram_tester_->ExpectTotalCount(kLoadedKeyTypeHistogram, times_loaded);
  }

  void ExpectKeyCreatedMetrics() {
    histogram_tester_->ExpectUniqueSample(kKeyCreationResultHistogram,
                                          DTKeyRotationResult::kSucceeded, 1);
    histogram_tester_->ExpectTotalCount(kKeyRotationResultHistogram, 0);
  }

  void ExpectKeyRotateMetrics(DTKeyRotationResult result,
                              int times_rotated = 1) {
    histogram_tester_->ExpectUniqueSample(kKeyRotationResultHistogram, result,
                                          times_rotated);
    histogram_tester_->ExpectTotalCount(kKeyCreationResultHistogram, 0);
  }

  void InitializeWithKey() {
    SetUpKeyLoadAndSync(DTCLoadKeyResult(kSuccessUploadCode, test_key_pair_));

    key_manager_->StartInitialization();

    ExpectManagerHandlesRequests();

    ExpectLoadedHardwareKeyMetrics();
    histogram_tester_->ExpectTotalCount(kKeyCreationResultHistogram, 0);
    histogram_tester_->ExpectTotalCount(kKeyRotationResultHistogram, 0);
  }

  // Expects that the key manager can handle an incoming request successfully,
  // which means it holds a valid key.
  void ExpectManagerHandlesRequests() {
    base::RunLoop run_loop;
    key_manager_->ExportPublicKeyAsync(base::BindLambdaForTesting(
        [&run_loop](std::optional<std::string> value) {
          EXPECT_TRUE(value);
          EXPECT_FALSE(value->empty());
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  void ResetState() {
    auto mock_launcher =
        std::make_unique<StrictMock<MockKeyRotationLauncher>>();
    mock_launcher_ = mock_launcher.get();

    auto mock_loader = std::make_unique<StrictMock<MockKeyLoader>>();
    mock_loader_ = mock_loader.get();

    key_manager_ = std::make_unique<DeviceTrustKeyManagerImpl>(
        std::move(mock_launcher), std::move(mock_loader));

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  DeviceTrustKeyManagerImpl* key_manager() { return key_manager_.get(); }
  StrictMock<MockKeyRotationLauncher>* mock_launcher() {
    return mock_launcher_;
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<DeviceTrustKeyManagerImpl> key_manager_;

  raw_ptr<StrictMock<MockKeyRotationLauncher>> mock_launcher_;

  raw_ptr<StrictMock<MockKeyLoader>> mock_loader_;

  scoped_refptr<SigningKeyPair> test_key_pair_;
};

// Tests that StartInitialization will load a key and not trigger key creation
// if key loading was successful.
TEST_F(DeviceTrustKeyManagerImplTest, Initialization_WithPersistedKey) {
  InitializeWithKey();

  auto key_metadata = key_manager()->GetLoadedKeyMetadata();
  ASSERT_TRUE(key_metadata);
  ASSERT_TRUE(key_metadata->synchronization_response_code);
  EXPECT_EQ(key_metadata->synchronization_response_code.value(),
            kSuccessUploadCode);
  EXPECT_FALSE(key_metadata->permanent_failure);

  EXPECT_FALSE(key_manager()->HasPermanentFailure());
}

// Tests that StartInitialization will load a key and not trigger key creation
// if key loading was successful.
TEST_F(DeviceTrustKeyManagerImplTest, SignString_HardwareKey) {
  InitializeWithKey();
  EXPECT_FALSE(key_manager()->HasPermanentFailure());

  base::test::TestFuture<std::optional<std::vector<uint8_t>>> sign_future;
  key_manager()->SignStringAsync("test string", sign_future.GetCallback());

  EXPECT_TRUE(sign_future.Get());

  static constexpr char kSignatureHistogramHw[] =
      "Enterprise.DeviceTrust.Key.Signing.Latency.Hardware";
  histogram_tester_->ExpectTotalCount(kSignatureHistogramHw, 1);
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful due to a nullptr for the key pair being returned.
// - Key creation succeeds and a key gets loaded successfully,
// - Then a client request gets replied successfully.
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_InitialNull_CreatesKey_LoadKeySuccess) {
  SetUpKeyLoadAndSync(DTCLoadKeyResult(LoadPersistedKeyResult::kNotFound));
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
  SetUpKeyLoadAndSync(DTCLoadKeyResult(kSuccessUploadCode, test_key_pair()));
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  // The manager should now respond to the callback as soon as the key is
  // loaded.
  base::RunLoop run_loop;
  key_manager()->ExportPublicKeyAsync(
      base::BindLambdaForTesting([&run_loop](std::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectLoadedHardwareKeyMetrics();
  ExpectKeyCreatedMetrics();
  EXPECT_FALSE(key_manager()->HasPermanentFailure());
}

// Tests that:
// - StartInitialization will trigger key creation if key loading was not
//   successful due to an empty key pair being returned.
// - Key creation succeeds and a key gets loaded successfully,
// - Then a client request gets replied successfully.
TEST_F(DeviceTrustKeyManagerImplTest,
       Initialization_InitialKeyEmpty_CreatesKey_LoadKeySuccess) {
  DTCLoadKeyResult load_key_result(LoadPersistedKeyResult::kNotFound);
  load_key_result.key_pair = base::MakeRefCounted<SigningKeyPair>(
      nullptr, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  SetUpKeyLoadAndSync(std::move(load_key_result));

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
  SetUpKeyLoadAndSync(DTCLoadKeyResult(kSuccessUploadCode, test_key_pair()));
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  // The manager should now respond to the callback as soon as the key is
  // loaded.
  base::RunLoop run_loop;
  key_manager()->ExportPublicKeyAsync(
      base::BindLambdaForTesting([&run_loop](std::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectLoadedHardwareKeyMetrics();
  ExpectKeyCreatedMetrics();
  EXPECT_FALSE(key_manager()->HasPermanentFailure());
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
  SetUpKeyLoadAndSync(DTCLoadKeyResult(LoadPersistedKeyResult::kNotFound));

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

  // Mimic that the key creation was successful, however the key loading mocks
  // still mimic a loading failure.
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  // The manager should now try to load the key upon receiving the client
  // request, but will fail to do so. It will then reply with a failure to the
  // client requests.
  base::RunLoop fail_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&fail_loop](std::optional<std::string> value) {
        EXPECT_FALSE(value);
        fail_loop.Quit();
      }));

  fail_loop.Run();

  // Retry, but with a successful key loading this time.
  SetUpKeyLoadAndSync(DTCLoadKeyResult(kSuccessUploadCode, test_key_pair()));
  base::RunLoop success_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&success_loop](std::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        success_loop.Quit();
      }));

  success_loop.Run();

  ExpectLoadedHardwareKeyMetrics();
  ExpectKeyCreatedMetrics();
}

struct LoadKeyResultTestCase {
  LoadPersistedKeyResult result{};
  bool triggers_creation{};
};

// Tests the various possible values of LoadPersistedKeyResult when no key was
// loaded and how they affect triggering key creation.
TEST_F(DeviceTrustKeyManagerImplTest, NoKey_LoadKeyResult_MayTriggerCreation) {
  std::array<LoadKeyResultTestCase, 4> test_cases = {
      LoadKeyResultTestCase{LoadPersistedKeyResult::kSuccess,
                            /*triggers_creation=*/false},
      LoadKeyResultTestCase{LoadPersistedKeyResult::kNotFound,
                            /*triggers_creation=*/true},
      LoadKeyResultTestCase{LoadPersistedKeyResult::kMalformedKey,
                            /*triggers_creation=*/true},
      LoadKeyResultTestCase{LoadPersistedKeyResult::kUnknown,
                            /*triggers_creation=*/false}};

  for (const auto& test_case : test_cases) {
    DTCLoadKeyResult load_key_result(test_case.result);

    base::RunLoop run_loop;
    if (test_case.triggers_creation) {
      SetUpKeyLoadAndSync(load_key_result);
      EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
          .WillOnce(Invoke(
              [&](const std::string& nonce,
                  KeyRotationCommand::Callback callback) { run_loop.Quit(); }));
    } else {
      base::RepeatingClosure side_effect =
          base::BindLambdaForTesting([&run_loop, this]() {
            key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
                [&run_loop](std::optional<std::string> value) {
                  EXPECT_FALSE(value);
                  run_loop.Quit();
                }));
          });
      SetUpKeyLoadAndSyncWithSideEffect(load_key_result, side_effect);

      EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
          .Times(0);
    }

    key_manager()->StartInitialization();

    run_loop.Run();
    RunUntilIdle();
    ResetState();
  }
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
  SetUpKeyLoadAndSync(DTCLoadKeyResult(LoadPersistedKeyResult::kNotFound));

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
  ASSERT_FALSE(failed_rotation_callback.is_null());
  std::move(failed_rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  histogram_tester_->ExpectUniqueSample(kKeyCreationResultHistogram,
                                        DTKeyRotationResult::kFailed, 1);

  KeyRotationCommand::Callback success_rotation_callback;
  base::RunLoop create_key_success_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            success_rotation_callback = std::move(callback);
            create_key_success_loop.Quit();
          }));

  // Should not be treated as a permanent failure.
  EXPECT_FALSE(key_manager()->HasPermanentFailure());

  // This client request will try to load the key, then fail (since key creation
  // failed previously), and then trigger a successful key creation followed
  // by a successful key loading.
  base::RunLoop request_loop;
  key_manager()->ExportPublicKeyAsync(base::BindLambdaForTesting(
      [&request_loop](std::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        request_loop.Quit();
      }));

  create_key_success_loop.Run();

  // Make the key creation return a successful status and fake that a key is
  // loadable.
  SetUpKeyLoadAndSync(DTCLoadKeyResult(kSuccessUploadCode, test_key_pair()));
  ASSERT_FALSE(success_rotation_callback.is_null());
  std::move(success_rotation_callback)
      .Run(KeyRotationCommand::Status::SUCCEEDED);

  // The client request should be responded to.
  request_loop.Run();

  ExpectLoadedHardwareKeyMetrics();
  histogram_tester_->ExpectTotalCount(kKeyRotationResultHistogram, 0);
  histogram_tester_->ExpectBucketCount(kKeyCreationResultHistogram,
                                       DTKeyRotationResult::kSucceeded, 1);
  EXPECT_FALSE(key_manager()->HasPermanentFailure());
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
  SetUpKeyLoadAndSync(DTCLoadKeyResult(LoadPersistedKeyResult::kNotFound));

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
                                     std::optional<std::string> value) {
        ASSERT_TRUE(value);
        EXPECT_FALSE(value->empty());
        ++export_key_counter;
        barrier_closure.Run();
      });

  auto sign_string_counter = 0;
  auto sign_string_callback = base::BindLambdaForTesting(
      [&sign_string_counter,
       &barrier_closure](std::optional<std::vector<uint8_t>> value) {
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
  SetUpKeyLoadAndSync(DTCLoadKeyResult(kSuccessUploadCode, test_key_pair()));
  ASSERT_FALSE(key_rotation_callback.is_null());
  std::move(key_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // Queue-up more requests, which should be executed normally.
  key_manager()->ExportPublicKeyAsync(export_key_callback);
  key_manager()->SignStringAsync(kFakeData, sign_string_callback);

  // All pending callbacks should get called now.
  barrier_loop.Run();

  ExpectLoadedHardwareKeyMetrics();
  ExpectKeyCreatedMetrics();
}

// Tests that a properly initialized key manager handles a successful rotate key
// request properly.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Simple_Success) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  std::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));

  rotate_key_loop.Run();

  // Make the key rotation return a successful status.
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // The key manager should now be properly setup.
  ExpectManagerHandlesRequests();
  ExpectKeyRotateMetrics(DTKeyRotationResult::kSucceeded);

  // The manager should have loaded a total of two keys.
  ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/2);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::SUCCESS);
}

// Tests that a properly initialized key manager handles a failing rotate key
// request properly.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Simple_Failed) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  std::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));

  rotate_key_loop.Run();

  // Make the key rotation return a failed status.
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  // The key manager should still be properly setup.
  ExpectManagerHandlesRequests();
  ExpectKeyRotateMetrics(DTKeyRotationResult::kFailed);
  EXPECT_FALSE(key_manager()->HasPermanentFailure());

  // The manager should have loaded a total of one key, the initial one.
  ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/1);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::FAILURE);
}

// Tests that a properly initialized key manager handles a failing rotate key
// request due to key conflict properly.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Simple_Failed_Key_Conflict) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  std::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));

  rotate_key_loop.Run();

  // Make the key rotation return a failed key conflict status.
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback)
      .Run(KeyRotationCommand::Status::FAILED_KEY_CONFLICT);
  RunUntilIdle();

  // The key manager should still be properly setup.
  ExpectManagerHandlesRequests();
  ExpectKeyRotateMetrics(DTKeyRotationResult::kFailedKeyConflict);

  // Conflict only leads to a permanent failure if it occurs during key
  // creation.
  EXPECT_FALSE(key_manager()->HasPermanentFailure());

  // The manager should have loaded a total of one key, the initial one.
  ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/1);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::FAILURE);
}

// Tests that a properly initialized key manager handles a failing rotate key
// request due to OS restrictions properly.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Simple_Failed_OS_Failure) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  std::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));

  rotate_key_loop.Run();

  // Make the key rotation return a failed OS restriction status.
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback)
      .Run(KeyRotationCommand::Status::FAILED_OS_RESTRICTION);
  RunUntilIdle();

  // The key manager should still be properly setup.
  ExpectManagerHandlesRequests();
  ExpectKeyRotateMetrics(DTKeyRotationResult::kFailedOSRestriction);

  // The manager should have loaded a total of one key, the initial one.
  ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/1);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::FAILURE);
}

// Tests that a properly initialized key manager handles concurrent successful
// rotate key request properly, which includes cancelling pending requests when
// another one is coming in.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Concurrent_Cancel_Success) {
  // A key already exists and the manager already loaded it.
  InitializeWithKey();

  KeyRotationCommand::Callback first_rotation_callback;
  base::RunLoop first_rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            first_rotation_callback = std::move(callback);
            first_rotate_key_loop.Quit();
          }));

  // Create callback parameters for all calls.
  std::optional<KeyRotationResult> first_captured_result;
  auto first_completion_callback = base::BindLambdaForTesting(
      [&first_captured_result](KeyRotationResult result) {
        first_captured_result = result;
      });
  std::optional<KeyRotationResult> second_captured_result;
  auto second_completion_callback = base::BindLambdaForTesting(
      [&second_captured_result](KeyRotationResult result) {
        second_captured_result = result;
      });
  std::optional<KeyRotationResult> third_captured_result;
  auto third_completion_callback = base::BindLambdaForTesting(
      [&third_captured_result](KeyRotationResult result) {
        third_captured_result = result;
      });

  // Kick off the concurrent rotation requests.
  key_manager()->RotateKey(kFakeNonce, std::move(first_completion_callback));
  key_manager()->RotateKey("irrelevant, random nonce.",
                           std::move(second_completion_callback));
  key_manager()->RotateKey(kOtherFakeNonce,
                           std::move(third_completion_callback));

  first_rotate_key_loop.Run();

  KeyRotationCommand::Callback second_rotation_callback;
  base::RunLoop second_rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kOtherFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            second_rotation_callback = std::move(callback);
            second_rotate_key_loop.Quit();
          }));

  // Make the key rotation return a successful status.
  ASSERT_FALSE(first_rotation_callback.is_null());
  std::move(first_rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);

  RunUntilIdle();

  second_rotate_key_loop.Run();

  // Make the second key rotation return a successful status.
  ASSERT_FALSE(second_rotation_callback.is_null());
  std::move(second_rotation_callback)
      .Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // The key manager should still be properly setup.
  ExpectManagerHandlesRequests();
  ExpectKeyRotateMetrics(DTKeyRotationResult::kSucceeded, /*times_rotated=*/2);

  // The manager should have loaded a total of three keys.
  ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/3);

  ASSERT_TRUE(first_captured_result.has_value());
  ASSERT_TRUE(second_captured_result.has_value());
  ASSERT_TRUE(third_captured_result.has_value());
  ASSERT_EQ(first_captured_result.value(), KeyRotationResult::SUCCESS);
  ASSERT_EQ(second_captured_result.value(), KeyRotationResult::CANCELLATION);
  ASSERT_EQ(third_captured_result.value(), KeyRotationResult::SUCCESS);
}

struct ConcurrentRotationFailureTestCase {
  KeyRotationCommand::Status failed_rotation_status{};
  DTKeyRotationResult metric_status{};
};

// Tests that a properly initialized key manager handles concurrent rotate key
// request properly when the second one fails.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_Concurrent_SuccessThenFail) {
  const std::array<ConcurrentRotationFailureTestCase, 8> test_cases = {
      ConcurrentRotationFailureTestCase{KeyRotationCommand::Status::FAILED,
                                        DTKeyRotationResult::kFailed},
      ConcurrentRotationFailureTestCase{
          KeyRotationCommand::Status::FAILED_OS_RESTRICTION,
          DTKeyRotationResult::kFailedOSRestriction},
      ConcurrentRotationFailureTestCase{
          KeyRotationCommand::Status::FAILED_KEY_CONFLICT,
          DTKeyRotationResult::kFailedKeyConflict},
      ConcurrentRotationFailureTestCase{
          KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN_STORAGE,
          DTKeyRotationResult::kFailedInvalidDmTokenStorage},
      ConcurrentRotationFailureTestCase{
          KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN,
          DTKeyRotationResult::kFailedInvalidDmToken},
      ConcurrentRotationFailureTestCase{
          KeyRotationCommand::Status::FAILED_INVALID_MANAGEMENT_SERVICE,
          DTKeyRotationResult::kFailedInvalidManagementService},
      ConcurrentRotationFailureTestCase{
          KeyRotationCommand::Status::FAILED_INVALID_DMSERVER_URL,
          DTKeyRotationResult::kFailedInvalidDmServerUrl},
      ConcurrentRotationFailureTestCase{
          KeyRotationCommand::Status::FAILED_INVALID_COMMAND,
          DTKeyRotationResult::kFailedInvalidCommand}};

  for (const auto& test_case : test_cases) {
    // A key already exists and the manager already loaded it.
    InitializeWithKey();

    KeyRotationCommand::Callback first_rotation_callback;
    base::RunLoop first_rotate_key_loop;
    EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
        .WillOnce(Invoke([&](const std::string& nonce,
                             KeyRotationCommand::Callback callback) {
          first_rotation_callback = std::move(callback);
          first_rotate_key_loop.Quit();
        }));

    // Create callback parameters for all calls.
    std::optional<KeyRotationResult> first_captured_result;
    auto first_completion_callback = base::BindLambdaForTesting(
        [&first_captured_result](KeyRotationResult result) {
          first_captured_result = result;
        });
    std::optional<KeyRotationResult> second_captured_result;
    auto second_completion_callback = base::BindLambdaForTesting(
        [&second_captured_result](KeyRotationResult result) {
          second_captured_result = result;
        });

    // Kick off the concurrent rotation requests.
    key_manager()->RotateKey(kFakeNonce, std::move(first_completion_callback));
    key_manager()->RotateKey(kOtherFakeNonce,
                             std::move(second_completion_callback));

    first_rotate_key_loop.Run();

    KeyRotationCommand::Callback second_rotation_callback;
    base::RunLoop second_rotate_key_loop;
    EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kOtherFakeNonce, _))
        .WillOnce(Invoke([&](const std::string& nonce,
                             KeyRotationCommand::Callback callback) {
          second_rotation_callback = std::move(callback);
          second_rotate_key_loop.Quit();
        }));

    // Make the key rotation return a successful status.
    ASSERT_FALSE(first_rotation_callback.is_null());
    std::move(first_rotation_callback)
        .Run(KeyRotationCommand::Status::SUCCEEDED);
    RunUntilIdle();

    second_rotate_key_loop.Run();

    // Make the second key rotation return a failed status.
    ASSERT_FALSE(second_rotation_callback.is_null());
    std::move(second_rotation_callback).Run(test_case.failed_rotation_status);
    RunUntilIdle();

    // The key manager should still be properly setup.
    ExpectManagerHandlesRequests();
    histogram_tester_->ExpectBucketCount(kKeyRotationResultHistogram,
                                         DTKeyRotationResult::kSucceeded, 1);
    histogram_tester_->ExpectBucketCount(kKeyRotationResultHistogram,
                                         test_case.metric_status, 1);
    histogram_tester_->ExpectTotalCount(kKeyRotationResultHistogram, 2);
    histogram_tester_->ExpectTotalCount(kKeyCreationResultHistogram, 0);

    // The manager should have loaded a total of two keys.
    ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/2);

    ASSERT_TRUE(first_captured_result.has_value());
    ASSERT_TRUE(second_captured_result.has_value());
    ASSERT_EQ(first_captured_result.value(), KeyRotationResult::SUCCESS);
    ASSERT_EQ(second_captured_result.value(), KeyRotationResult::FAILURE);

    ResetState();
  }
}

// Tests that a properly initialized key manager handles a successful rotate key
// request properly when it receives it while already loading a key.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_AtLoadKey_Success) {
  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  // Binding the rotate request to the main thread, as the sequence checker will
  // be expecting that.
  std::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  base::RepeatingClosure start_rotate =
      base::BindPostTaskToCurrentDefault(base::BindLambdaForTesting([&]() {
        key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));
      }));

  // Setup so that a key is loadable, but a rotate request is received at the
  // same time as it is being loaded.
  SetUpKeyLoadAndSyncWithSideEffect(start_rotate);

  // Starting initialization will start loading the key.
  key_manager()->StartInitialization();

  rotate_key_loop.Run();

  // Make the key rotation return a successful status and fake that a key is
  // loadable.
  SetUpKeyLoadAndSync(DTCLoadKeyResult(kSuccessUploadCode, test_key_pair()));
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::SUCCEEDED);
  RunUntilIdle();

  // The key manager should now be properly setup.
  ExpectManagerHandlesRequests();
  ExpectKeyRotateMetrics(DTKeyRotationResult::kSucceeded);

  // The manager should have loaded a total of two keys.
  ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/2);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::SUCCESS);
}

// Tests that a properly initialized key manager handles a failed rotate key
// request properly when it receives it while already loading a key.
TEST_F(DeviceTrustKeyManagerImplTest, RotateKey_AtLoadKey_Fails) {
  KeyRotationCommand::Callback rotation_callback;
  base::RunLoop rotate_key_loop;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(kFakeNonce, _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            rotation_callback = std::move(callback);
            rotate_key_loop.Quit();
          }));

  // Binding the rotate request to the main thread, as the sequence checker will
  // be expecting that.
  std::optional<KeyRotationResult> captured_result;
  auto completion_callback =
      base::BindLambdaForTesting([&captured_result](KeyRotationResult result) {
        captured_result = result;
      });
  base::RepeatingClosure start_rotate =
      base::BindPostTaskToCurrentDefault(base::BindLambdaForTesting([&]() {
        key_manager()->RotateKey(kFakeNonce, std::move(completion_callback));
      }));

  // Setup so that a key is loadable, but a rotate request is received at the
  // same time as it is being loaded.
  SetUpKeyLoadAndSyncWithSideEffect(start_rotate);

  // Starting initialization will start loading the key.
  key_manager()->StartInitialization();

  rotate_key_loop.Run();

  // Make the key rotation return a failed status.
  ASSERT_FALSE(rotation_callback.is_null());
  std::move(rotation_callback).Run(KeyRotationCommand::Status::FAILED);
  RunUntilIdle();

  // The key manager should still be properly setup (using the old key).
  ExpectManagerHandlesRequests();
  ExpectKeyRotateMetrics(DTKeyRotationResult::kFailed);

  // The manager should have loaded a total of one key.
  ExpectLoadedHardwareKeyMetrics(/*times_loaded=*/1);

  ASSERT_TRUE(captured_result.has_value());
  ASSERT_EQ(captured_result.value(), KeyRotationResult::FAILURE);
}

struct PermanentFailureTestCase {
  KeyRotationCommand::Status failed_rotation_status{};
  DTKeyRotationResult metric_status{};
  PermanentFailure permanent_failure{};
};

// Tests that a key manager disables retries whenever it encounters a permanent
// failure during key creation.
TEST_F(DeviceTrustKeyManagerImplTest, CreateKey_PermanentFailures) {
  const std::array<PermanentFailureTestCase, 4> test_cases = {
      PermanentFailureTestCase{KeyRotationCommand::Status::FAILED_KEY_CONFLICT,
                               DTKeyRotationResult::kFailedKeyConflict,
                               PermanentFailure::kCreationUploadConflict},
      PermanentFailureTestCase{
          KeyRotationCommand::Status::FAILED_OS_RESTRICTION,
          DTKeyRotationResult::kFailedOSRestriction,
          PermanentFailure::kOsRestriction},
      PermanentFailureTestCase{
          KeyRotationCommand::Status::FAILED_INVALID_PERMISSIONS,
          DTKeyRotationResult::kFailedInvalidPermissions,
          PermanentFailure::kInsufficientPermissions},
      PermanentFailureTestCase{
          KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION,
          DTKeyRotationResult::kFailedInvalidInstallation,
          PermanentFailure::kInvalidInstallation}};

  for (const auto& test_case : test_cases) {
    SetUpKeyLoadAndSync(DTCLoadKeyResult(LoadPersistedKeyResult::kNotFound));

    KeyRotationCommand::Callback failed_rotation_callback;
    base::RunLoop create_key_fail_loop;
    EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
        .WillOnce(Invoke([&](const std::string& nonce,
                             KeyRotationCommand::Callback callback) {
          failed_rotation_callback = std::move(callback);
          create_key_fail_loop.Quit();
        }));

    key_manager()->StartInitialization();

    create_key_fail_loop.Run();

    // Mimic that key creation failed with a permanent failure.
    ASSERT_FALSE(failed_rotation_callback.is_null());
    std::move(failed_rotation_callback).Run(test_case.failed_rotation_status);
    RunUntilIdle();

    histogram_tester_->ExpectUniqueSample(kKeyCreationResultHistogram,
                                          test_case.metric_status, 1);

    EXPECT_TRUE(key_manager()->HasPermanentFailure());

    const auto& key_metadata = key_manager()->GetLoadedKeyMetadata();
    ASSERT_TRUE(key_metadata);
    ASSERT_TRUE(key_metadata->permanent_failure);

    EXPECT_EQ(key_metadata->permanent_failure.value(),
              test_case.permanent_failure);

    // All operations of the key manager should fail without launching the key
    // creation.
    key_manager()->StartInitialization();

    base::test::TestFuture<KeyRotationResult> rotate_future;
    key_manager()->RotateKey(kFakeNonce, rotate_future.GetCallback());
    EXPECT_EQ(rotate_future.Get(), KeyRotationResult::FAILURE);

    base::test::TestFuture<std::optional<std::string>> export_future;
    key_manager()->ExportPublicKeyAsync(export_future.GetCallback());
    EXPECT_FALSE(export_future.Get());

    base::test::TestFuture<std::optional<std::vector<uint8_t>>> sign_future;
    key_manager()->SignStringAsync("test string", sign_future.GetCallback());
    EXPECT_FALSE(sign_future.Get());

    ResetState();
  }
}

// Tests the case where a key creation results in a permanent failure while a
// concurrent rotation request was received.
TEST_F(DeviceTrustKeyManagerImplTest,
       CreateKeyPermanentFailure_ConcurrentRotate) {
  SetUpKeyLoadAndSync(DTCLoadKeyResult(LoadPersistedKeyResult::kNotFound));

  base::RunLoop create_key_loop;
  KeyRotationCommand::Callback key_creation_callback;
  EXPECT_CALL(*mock_launcher(), LaunchKeyRotation(std::string(), _))
      .WillOnce(Invoke(
          [&](const std::string& nonce, KeyRotationCommand::Callback callback) {
            key_creation_callback = std::move(callback);
            create_key_loop.Quit();
          }));

  key_manager()->StartInitialization();

  create_key_loop.Run();

  ASSERT_FALSE(key_creation_callback.is_null());

  // Key manager is now waiting for the creation to finish, and will therefore
  // mark the rotation request as pending.
  base::test::TestFuture<KeyRotationResult> rotate_future;
  key_manager()->RotateKey(kFakeNonce, rotate_future.GetCallback());

  // Fake as if the creation failed with a permanent failure.
  std::move(key_creation_callback)
      .Run(KeyRotationCommand::Status::FAILED_KEY_CONFLICT);
  RunUntilIdle();

  EXPECT_TRUE(key_manager()->HasPermanentFailure());
  EXPECT_EQ(rotate_future.Get(), KeyRotationResult::FAILURE);
}

}  // namespace enterprise_connectors
