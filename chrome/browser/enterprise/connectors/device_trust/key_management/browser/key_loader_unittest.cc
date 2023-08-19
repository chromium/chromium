// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using DTCLoadKeyResult = KeyLoader::DTCLoadKeyResult;
using HttpResponseCode = KeyNetworkDelegate::HttpResponseCode;

using test::MockKeyNetworkDelegate;
using test::MockKeyPersistenceDelegate;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace {
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kFakeClientId[] = "fake-client-id";
constexpr char kExpectedDmServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";

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

    auto mock_network_delegate =
        std::make_unique<StrictMock<MockKeyNetworkDelegate>>();
    mock_network_delegate_ = mock_network_delegate.get();
    loader_ = std::make_unique<KeyLoaderImpl>(&fake_dm_token_storage_,
                                              &fake_device_management_service_,
                                              std::move(mock_network_delegate));
  }

  void SetDMToken() {
    // Set valid values.
    fake_dm_token_storage_.SetDMToken(kFakeDMToken);
    fake_dm_token_storage_.SetClientId(kFakeClientId);
  }

  void SetPersistedKey(bool has_key = true) {
    auto mock_persistence_delegate =
        std::make_unique<StrictMock<MockKeyPersistenceDelegate>>();
    EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair(_))
        .WillOnce(Return(has_key ? test_key_pair_ : nullptr));
    persistence_delegate_factory_.set_next_instance(
        std::move(mock_persistence_delegate));
  }

  void SetUploadCode(HttpResponseCode response_code = kSuccessCode) {
    EXPECT_CALL(
        *mock_network_delegate_,
        SendPublicKeyToDmServer(GURL(kExpectedDmServerUrl), kFakeDMToken, _, _))
        .WillOnce(Invoke(
            [&, response_code](const GURL& url, const std::string& dm_token,
                               const std::string& body,
                               base::OnceCallback<void(int)> callback) {
              std::move(callback).Run(response_code);
            }));
  }

  void RunAndValidateLoadKey(DTCLoadKeyResult result) {
    base::test::TestFuture<DTCLoadKeyResult> future;
    loader_->LoadKey(future.GetCallback());

    EXPECT_EQ(future.Get().key_pair, result.key_pair);
    EXPECT_EQ(future.Get().status_code, result.status_code);
  }

  base::test::TaskEnvironment task_environment_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService fake_device_management_service_{
      &job_creation_handler_};
  raw_ptr<StrictMock<MockKeyNetworkDelegate>, DanglingUntriaged>
      mock_network_delegate_;
  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
  scoped_refptr<SigningKeyPair> test_key_pair_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::unique_ptr<KeyLoader> loader_;
  base::HistogramTester histogram_tester_;
};

TEST_F(KeyLoaderTest, CreateKeyLoader_Success) {
  EXPECT_TRUE(KeyLoader::Create(&fake_dm_token_storage_,
                                &fake_device_management_service_,
                                test_shared_loader_factory_));
}

TEST_F(KeyLoaderTest, CreateKeyLoader_InvalidURLLoaderFactory) {
  auto loader = KeyLoader::Create(&fake_dm_token_storage_,
                                  &fake_device_management_service_, nullptr);
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(loader);
#else
  EXPECT_FALSE(loader);
#endif  // BUILDFLAG(IS_WIN)
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
  fake_dm_token_storage_.SetDMToken("");
  SetPersistedKey();

  RunAndValidateLoadKey(DTCLoadKeyResult());

  histogram_tester_.ExpectUniqueSample(kSynchronizationErrorHistogram,
                                       DTSynchronizationError::kInvalidDmToken,
                                       1);
  histogram_tester_.ExpectTotalCount(kSynchronizationUploadHistogram, 0);
}

TEST_F(KeyLoaderTest, LoadKey_MissingKeyPair) {
  SetDMToken();
  SetPersistedKey(/*has_key=*/false);

  RunAndValidateLoadKey(DTCLoadKeyResult());

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
