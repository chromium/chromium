// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/mock_cloud_management_delegate.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using DTCLoadKeyResult = KeyLoader::DTCLoadKeyResult;
using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

using test::MockKeyPersistenceDelegate;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace {
constexpr char kFakeDMToken[] = "fake-browser-dm-token";

constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kHardFailure = 400;

constexpr char kSynchronizationErrorHistogram[] =
    "Enterprise.DeviceTrust.SyncSigningKey.ClientError";
constexpr char kSynchronizationUploadHistogram[] =
    "Enterprise.DeviceTrust.SyncSigningKey.UploadCode";

scoped_refptr<SigningKeyPair> CreateFakeKeyPair() {
  ECSigningKeyProvider provider;
  auto algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider.GenerateSigningKeySlowly(algorithm);
  DCHECK(signing_key);
  return base::MakeRefCounted<SigningKeyPair>(std::move(signing_key),
                                              BPKUR::CHROME_BROWSER_OS_KEY);
}

}  // namespace

class KeyLoaderTest : public testing::Test {
 protected:
  KeyLoaderTest() {
    test_key_pair_ = CreateFakeKeyPair();

    auto mock_management_delegate = std::make_unique<
        StrictMock<enterprise_attestation::MockCloudManagementDelegate>>();
    mock_management_delegate_ = mock_management_delegate.get();
    loader_ =
        std::make_unique<KeyLoaderImpl>(std::move(mock_management_delegate));
  }

  void SetDMToken(std::optional<std::string> dm_token = kFakeDMToken) {
    EXPECT_CALL(*mock_management_delegate_, GetDMToken())
        .WillRepeatedly(Return(dm_token));
  }

  void SetPersistedKey(bool has_key = true) {
    auto mock_persistence_delegate =
        std::make_unique<StrictMock<MockKeyPersistenceDelegate>>();
    EXPECT_CALL(*mock_persistence_delegate,
                LoadKeyPair(KeyStorageType::kPermanent, _))
        .WillOnce(Invoke([this, has_key](KeyStorageType key_type,
                                         LoadPersistedKeyResult* result) {
          if (has_key) {
            *result = LoadPersistedKeyResult::kSuccess;
            return test_key_pair_;
          }
          *result = LoadPersistedKeyResult::kNotFound;
          return scoped_refptr<SigningKeyPair>(nullptr);
        }));
    persistence_delegate_factory_.set_next_instance(
        std::move(mock_persistence_delegate));
  }

  void SetUploadCode(HttpResponseCode response_code = kSuccessCode) {
    policy::DMServerJobResult result;
    result.response_code = response_code;
    EXPECT_CALL(*mock_management_delegate_, UploadBrowserPublicKey(_, _))
        .WillOnce(Invoke(
            [&, result](
                const enterprise_management::DeviceManagementRequest& request,
                base::OnceCallback<void(policy::DMServerJobResult)> callback) {
              std::move(callback).Run(result);
            }));
  }

  void RunAndValidateLoadKey(DTCLoadKeyResult expected_result) {
    base::test::TestFuture<DTCLoadKeyResult> future;
    loader_->LoadKey(future.GetCallback());

    const auto& loaded_key_result = future.Get();
    EXPECT_EQ(loaded_key_result.key_pair, expected_result.key_pair);
    EXPECT_EQ(loaded_key_result.status_code, expected_result.status_code);
    EXPECT_EQ(loaded_key_result.result, expected_result.result);
  }

  base::test::TaskEnvironment task_environment_;
  StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService fake_device_management_service_{
      &job_creation_handler_};
  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
  scoped_refptr<SigningKeyPair> test_key_pair_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::unique_ptr<KeyLoader> loader_;
  raw_ptr<StrictMock<enterprise_attestation::MockCloudManagementDelegate>>
      mock_management_delegate_;
  base::HistogramTester histogram_tester_;
};

TEST_F(KeyLoaderTest, CreateKeyLoader_Success) {
  EXPECT_TRUE(KeyLoader::Create(&fake_device_management_service_,
                                test_shared_loader_factory_));
}

TEST_F(KeyLoaderTest, CreateKeyLoader_InvalidURLLoaderFactory) {
  auto loader = KeyLoader::Create(&fake_device_management_service_, nullptr);
  EXPECT_FALSE(loader);
}

TEST_F(KeyLoaderTest, LoadKey_Success) {
  SetDMToken();
  SetPersistedKey();
  SetUploadCode();

  RunAndValidateLoadKey(DTCLoadKeyResult(kSuccessCode, test_key_pair_));

  histogram_tester_.ExpectUniqueSample(kSynchronizationUploadHistogram,
                                       kSuccessCode, 1);
  histogram_tester_.ExpectTotalCount(kSynchronizationErrorHistogram, 0);
}

TEST_F(KeyLoaderTest, LoadKey_InvalidDMToken) {
  SetDMToken("");
  SetPersistedKey();

  RunAndValidateLoadKey(DTCLoadKeyResult(test_key_pair_));

  histogram_tester_.ExpectUniqueSample(kSynchronizationErrorHistogram,
                                       DTSynchronizationError::kInvalidDmToken,
                                       1);
  histogram_tester_.ExpectTotalCount(kSynchronizationUploadHistogram, 0);
}

TEST_F(KeyLoaderTest, LoadKey_MissingKeyPair) {
  SetDMToken();
  SetPersistedKey(/*has_key=*/false);

  RunAndValidateLoadKey(DTCLoadKeyResult(LoadPersistedKeyResult::kNotFound));

  histogram_tester_.ExpectUniqueSample(kSynchronizationErrorHistogram,
                                       DTSynchronizationError::kMissingKeyPair,
                                       1);
  histogram_tester_.ExpectTotalCount(kSynchronizationUploadHistogram, 0);
}

TEST_F(KeyLoaderTest, LoadKey_KeyUploadFailed) {
  SetDMToken();
  SetPersistedKey();
  SetUploadCode(kHardFailure);

  RunAndValidateLoadKey(DTCLoadKeyResult(kHardFailure, test_key_pair_));

  histogram_tester_.ExpectUniqueSample(kSynchronizationUploadHistogram,
                                       kHardFailure, 1);
  histogram_tester_.ExpectTotalCount(kSynchronizationErrorHistogram, 0);
}

}  // namespace enterprise_connectors
