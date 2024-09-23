// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using testing::_;
using testing::ByMove;
using testing::ElementsAre;
using testing::InSequence;
using testing::Invoke;
using testing::Not;
using testing::Pair;
using testing::Return;
using testing::StrictMock;

namespace enterprise_connectors {

using test::MockKeyNetworkDelegate;
using test::MockKeyPersistenceDelegate;
using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

namespace {

constexpr char kDmServerUrl[] = "https://dmserver.example.com";
constexpr char kDmToken[] = "dm_token";
constexpr char kFakeNonce[] = "nonce";

constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kHardFailureCode = 400;
constexpr HttpResponseCode kKeyConflictFailureCode = 409;
constexpr HttpResponseCode kTransientFailureCode = 500;

constexpr char kRotateStatusWithNonceHistogram[] =
    "Enterprise.DeviceTrust.RotateSigningKey.WithNonce.Status";
constexpr char kRotateStatusNoNonceHistogram[] =
    "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.Status";
constexpr char kUploadCodeWithNonceHistogram[] =
    "Enterprise.DeviceTrust.RotateSigningKey.WithNonce.UploadCode";
constexpr char kUploadCodeNoNonceHistogram[] =
    "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.UploadCode";

constexpr char kHistogramPrefix[] = "Enterprise.DeviceTrust.RotateSigningKey";

// All use-cases of upload failures resulting in the key rotation manager
// attempting to rollback any local state.
constexpr std::array<
    std::tuple<HttpResponseCode, RotationStatus, KeyRotationResult, bool>,
    6>
    kUploadFailureTestCases = {{
        {kHardFailureCode, RotationStatus::FAILURE_CANNOT_UPLOAD_KEY,
         KeyRotationResult::kFailed, true},
        {kTransientFailureCode,
         RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED,
         KeyRotationResult::kFailed, true},
        {kKeyConflictFailureCode, RotationStatus::FAILURE_CANNOT_UPLOAD_KEY,
         KeyRotationResult::kFailedKeyConflict, true},
        {kHardFailureCode,
         RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED,
         KeyRotationResult::kFailed, false},
        {kTransientFailureCode,
         RotationStatus::
             FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED_RESTORE_FAILED,
         KeyRotationResult::kFailed, false},
        {kKeyConflictFailureCode,
         RotationStatus::FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED,
         KeyRotationResult::kFailedKeyConflict, false},
    }};

}  // namespace

class KeyRotationManagerTest : public testing::Test {
 protected:
  KeyRotationManagerTest()
      : key_provider_(crypto::GetUnexportableKeyProvider(/*config=*/{})) {
    ResetHistograms();
    auto mock_network_delegate =
        std::make_unique<StrictMock<MockKeyNetworkDelegate>>();
    mock_network_delegate_ = mock_network_delegate.get();

    auto mock_persistence_delegate =
        std::make_unique<StrictMock<MockKeyPersistenceDelegate>>();
    mock_persistence_delegate_ = mock_persistence_delegate.get();

    key_rotation_manager_ = KeyRotationManager::CreateForTesting(
        std::move(mock_network_delegate), std::move(mock_persistence_delegate));
  }

  std::unique_ptr<crypto::UnexportableSigningKey> CreateHardwareKey() {
    auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
    return key_provider_->GenerateSigningKeySlowly(acceptable_algorithms);
  }

  void SetUploadCode(HttpResponseCode response_code) {
    EXPECT_CALL(*mock_network_delegate_,
                SendPublicKeyToDmServer(GURL(kDmServerUrl), kDmToken, _, _))
        .WillOnce(Invoke(
            [&, response_code](const GURL& url, const std::string& dm_token,
                               const std::string& body,
                               base::OnceCallback<void(int)> callback) {
              captured_upload_body_ = body;
              std::move(callback).Run(response_code);
            }));
  }

  void SetUpOldKey(bool exists = true) {
    if (exists) {
      old_key_pair_ = base::MakeRefCounted<SigningKeyPair>(
          CreateHardwareKey(), BPKUR::CHROME_BROWSER_HW_KEY);
      EXPECT_CALL(*mock_persistence_delegate_,
                  LoadKeyPair(KeyStorageType::kPermanent, _))
          .WillOnce(Return(old_key_pair_));
    } else {
      old_key_pair_.reset();
      EXPECT_CALL(*mock_persistence_delegate_,
                  LoadKeyPair(KeyStorageType::kPermanent, _))
          .WillOnce(Invoke([]() { return nullptr; }));
    }
  }

  void SetRotationPermissions(bool success = true) {
    EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
        .WillOnce(Return(success));
  }

  void SetUpNewKeyCreation(bool success = true) {
    if (success) {
      new_key_pair_ = base::MakeRefCounted<SigningKeyPair>(
          CreateHardwareKey(), BPKUR::CHROME_BROWSER_HW_KEY);
      EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair())
          .WillOnce(Return(new_key_pair_));
    } else {
      EXPECT_CALL(*mock_persistence_delegate_, CreateKeyPair())
          .WillOnce(Invoke([]() { return nullptr; }));
    }
  }

  void SetUpStoreKey(bool expect_new_key, bool success) {
    std::vector<uint8_t> wrapped_key;
    if (expect_new_key) {
      ASSERT_TRUE(new_key_pair_);
      ASSERT_FALSE(new_key_pair_->is_empty());
      wrapped_key = new_key_pair_->key()->GetWrappedKey();
    } else {
      ASSERT_TRUE(old_key_pair_);
      ASSERT_FALSE(old_key_pair_->is_empty());
      wrapped_key = old_key_pair_->key()->GetWrappedKey();
    }

    EXPECT_CALL(*mock_persistence_delegate_,
                StoreKeyPair(BPKUR::CHROME_BROWSER_HW_KEY, wrapped_key))
        .WillOnce(Return(success));
  }

  void ExpectFinalCleanup() {
    EXPECT_CALL(*mock_persistence_delegate_, CleanupTemporaryKeyData())
        .Times(1);
  }

  void ExpectClearKey(bool success = true) {
    EXPECT_CALL(*mock_persistence_delegate_,
                StoreKeyPair(BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED,
                             std::vector<uint8_t>()))
        .WillOnce(Return(success));
  }

  void RunRotate(KeyRotationResult expected_result, bool with_nonce = false) {
    base::test::TestFuture<KeyRotationResult> future;
    key_rotation_manager_->Rotate(GURL(kDmServerUrl), kDmToken,
                                  with_nonce ? kFakeNonce : std::string(),
                                  future.GetCallback());
    EXPECT_EQ(expected_result, future.Get());
  }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  base::test::TaskEnvironment task_environment_;
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider_;

  raw_ptr<StrictMock<MockKeyNetworkDelegate>, DanglingUntriaged>
      mock_network_delegate_;
  raw_ptr<StrictMock<MockKeyPersistenceDelegate>, DanglingUntriaged>
      mock_persistence_delegate_;

  scoped_refptr<SigningKeyPair> old_key_pair_;
  scoped_refptr<SigningKeyPair> new_key_pair_;
  std::optional<std::string> captured_upload_body_;

  std::unique_ptr<KeyRotationManager> key_rotation_manager_;
};

TEST_F(KeyRotationManagerTest, Rotate_InvalidDMServerURL) {
  SetUpOldKey(/*exists=*/true);

  base::test::TestFuture<KeyRotationResult> future;
  key_rotation_manager_->Rotate(GURL(), kDmToken, kFakeNonce,
                                future.GetCallback());
  EXPECT_EQ(KeyRotationResult::kFailed, future.Get());

  histogram_tester_->ExpectUniqueSample(
      kRotateStatusWithNonceHistogram,
      RotationStatus::FAILURE_INVALID_DMSERVER_URL, 1);

  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusWithNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, Rotate_InvalidDmToken) {
  SetUpOldKey(/*exists=*/true);

  // Create a DM token that has 5000 characters.
  std::string long_dm_token(5000, 'a');

  base::test::TestFuture<KeyRotationResult> future;
  key_rotation_manager_->Rotate(GURL(kDmServerUrl), long_dm_token, kFakeNonce,
                                future.GetCallback());
  EXPECT_EQ(KeyRotationResult::kFailed, future.Get());

  histogram_tester_->ExpectUniqueSample(kRotateStatusWithNonceHistogram,
                                        RotationStatus::FAILURE_INVALID_DMTOKEN,
                                        1);

  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusWithNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, Rotate_MissingNonce) {
  SetUpOldKey(/*exists=*/true);

  RunRotate(KeyRotationResult::kFailed, /*with_nonce=*/false);

  histogram_tester_->ExpectUniqueSample(
      kRotateStatusWithNonceHistogram,
      RotationStatus::FAILURE_INVALID_ROTATION_PARAMS, 1);

  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusWithNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, CreateKey_InvalidPermissions) {
  SetUpOldKey(/*exists=*/false);
  SetRotationPermissions(false);

  RunRotate(KeyRotationResult::kInsufficientPermissions);

  histogram_tester_->ExpectUniqueSample(
      kRotateStatusNoNonceHistogram,
      RotationStatus::FAILURE_INCORRECT_FILE_PERMISSIONS, 1);

  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusNoNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, CreateKey_CreationFailure) {
  SetUpOldKey(/*exists=*/false);
  SetRotationPermissions();
  SetUpNewKeyCreation(/*success=*/false);

  RunRotate(KeyRotationResult::kFailed);

  histogram_tester_->ExpectUniqueSample(
      kRotateStatusNoNonceHistogram,
      RotationStatus::FAILURE_CANNOT_GENERATE_NEW_KEY, 1);

  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusNoNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, CreateKey_StoreFailed) {
  SetUpOldKey(/*exists=*/false);
  SetRotationPermissions();
  SetUpNewKeyCreation();
  SetUpStoreKey(/*expect_new_key=*/true, /*success=*/false);

  RunRotate(KeyRotationResult::kFailed);

  histogram_tester_->ExpectUniqueSample(
      kRotateStatusNoNonceHistogram, RotationStatus::FAILURE_CANNOT_STORE_KEY,
      1);

  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusNoNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, CreateKey_Success) {
  SetUpOldKey(/*exists=*/false);
  SetRotationPermissions();
  SetUpNewKeyCreation();
  SetUpStoreKey(/*expect_new_key=*/true, /*success=*/true);
  SetUploadCode(kSuccessCode);
  ExpectFinalCleanup();

  RunRotate(KeyRotationResult::kSucceeded);

  // Validate body.
  // TODO(b:254072094): Improve body content validation logic.
  ASSERT_TRUE(captured_upload_body_);
  enterprise_management::DeviceManagementRequest request;
  ASSERT_TRUE(request.ParseFromString(captured_upload_body_.value()));
  auto upload_key_request = request.browser_public_key_upload_request();
  EXPECT_EQ(BPKUR::EC_KEY, upload_key_request.key_type());
  EXPECT_EQ(BPKUR::CHROME_BROWSER_HW_KEY, upload_key_request.key_trust_level());
  EXPECT_FALSE(upload_key_request.public_key().empty());
  EXPECT_FALSE(upload_key_request.signature().empty());

  // Should expect one successful attempt to rotate a key.
  histogram_tester_->ExpectUniqueSample(kRotateStatusNoNonceHistogram,
                                        RotationStatus::SUCCESS, 1);
  histogram_tester_->ExpectUniqueSample(kUploadCodeNoNonceHistogram,
                                        kSuccessCode, 1);

  // Make sure no other histograms were logged.
  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusNoNonceHistogram, 1),
                          Pair(kUploadCodeNoNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, RotateKey_Success) {
  SetUpOldKey();
  SetRotationPermissions();
  SetUpNewKeyCreation();
  SetUpStoreKey(/*expect_new_key=*/true, /*success=*/true);
  SetUploadCode(kSuccessCode);
  ExpectFinalCleanup();

  RunRotate(KeyRotationResult::kSucceeded, /*with_nonce=*/true);

  // Validate body.
  // TODO(b:254072094): Improve body content validation logic.
  ASSERT_TRUE(captured_upload_body_);
  enterprise_management::DeviceManagementRequest request;
  ASSERT_TRUE(request.ParseFromString(captured_upload_body_.value()));
  auto upload_key_request = request.browser_public_key_upload_request();
  EXPECT_EQ(BPKUR::EC_KEY, upload_key_request.key_type());
  EXPECT_EQ(BPKUR::CHROME_BROWSER_HW_KEY, upload_key_request.key_trust_level());
  EXPECT_FALSE(upload_key_request.public_key().empty());
  EXPECT_FALSE(upload_key_request.signature().empty());

  // Should expect one successful attempt to rotate a key.
  histogram_tester_->ExpectUniqueSample(kRotateStatusWithNonceHistogram,
                                        RotationStatus::SUCCESS, 1);
  histogram_tester_->ExpectUniqueSample(kUploadCodeWithNonceHistogram,
                                        kSuccessCode, 1);

  // Make sure no other histograms were logged.
  EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
              ElementsAre(Pair(kRotateStatusWithNonceHistogram, 1),
                          Pair(kUploadCodeWithNonceHistogram, 1)));
}

TEST_F(KeyRotationManagerTest, CreateKey_UploadFailed) {
  for (const auto& test_case : kUploadFailureTestCases) {
    auto http_code = std::get<0>(test_case);
    auto rotation_status = std::get<1>(test_case);
    auto result_code = std::get<2>(test_case);
    auto cleanup_success = std::get<3>(test_case);

    InSequence s;

    SetUpOldKey(/*exists=*/false);
    SetRotationPermissions();
    SetUpNewKeyCreation();
    SetUpStoreKey(/*expect_new_key=*/true, /*success=*/true);
    SetUploadCode(http_code);
    ExpectClearKey(cleanup_success);

    RunRotate(result_code);

    histogram_tester_->ExpectUniqueSample(kRotateStatusNoNonceHistogram,
                                          rotation_status, 1);
    histogram_tester_->ExpectUniqueSample(kUploadCodeNoNonceHistogram,
                                          http_code, 1);

    // Make sure no other histograms were logged.
    EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
                ElementsAre(Pair(kRotateStatusNoNonceHistogram, 1),
                            Pair(kUploadCodeNoNonceHistogram, 1)));

    ResetHistograms();
  }
}

TEST_F(KeyRotationManagerTest, RotateKey_UploadFailed) {
  for (const auto& test_case : kUploadFailureTestCases) {
    auto http_code = std::get<0>(test_case);
    auto rotation_status = std::get<1>(test_case);
    auto result_code = std::get<2>(test_case);
    auto cleanup_success = std::get<3>(test_case);

    InSequence s;

    SetUpOldKey(/*exists=*/true);
    SetRotationPermissions();
    SetUpNewKeyCreation();
    SetUpStoreKey(/*expect_new_key=*/true, /*success=*/true);
    SetUploadCode(http_code);
    SetUpStoreKey(/*expect_new_key=*/false, /*success=*/cleanup_success);

    RunRotate(result_code, /*with_nonce=*/true);

    histogram_tester_->ExpectUniqueSample(kRotateStatusWithNonceHistogram,
                                          rotation_status, 1);
    histogram_tester_->ExpectUniqueSample(kUploadCodeWithNonceHistogram,
                                          http_code, 1);

    // Make sure no other histograms were logged.
    EXPECT_THAT(histogram_tester_->GetTotalCountsForPrefix(kHistogramPrefix),
                ElementsAre(Pair(kRotateStatusWithNonceHistogram, 1),
                            Pair(kUploadCodeWithNonceHistogram, 1)));

    ResetHistograms();
  }
}

}  // namespace enterprise_connectors
