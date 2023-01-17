// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mock_key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
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

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {
// Add a couple of seconds to the exact timeout time.
const base::TimeDelta kTimeoutTime =
    timeouts::kKeyUploadTimeout + base::Seconds(2);

constexpr char kNonce[] = "nonce";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kFakeClientId[] = "fake-client-id";
constexpr char kExpectedDmServerUrl[] =
    "https://example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";

constexpr char kSynchronizationErrorHistogram[] =
    "Enterprise.DeviceTrust.SyncSigningKey.ClientError";
constexpr char kSynchronizationUploadHistogram[] =
    "Enterprise.DeviceTrust.SyncSigningKey.UploadCode";

std::unique_ptr<SigningKeyPair> CreateFakeKeyPair() {
  ECSigningKeyProvider provider;
  auto algorithm = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider.GenerateSigningKeySlowly(algorithm);
  DCHECK(signing_key);
  return std::make_unique<SigningKeyPair>(std::move(signing_key),
                                          BPKUR::CHROME_BROWSER_OS_KEY);
}

}  // namespace

class KeyRotationLauncherTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_command =
        std::make_unique<testing::StrictMock<test::MockKeyRotationCommand>>();
    mock_command_ = mock_command.get();
    scoped_command_factory_.SetMock(std::move(mock_command));

    test_key_pair_ = CreateFakeKeyPair();

    launcher_ = KeyRotationLauncher::Create(&fake_dm_token_storage_,
                                            &fake_device_management_service_,
                                            test_shared_loader_factory_);
  }

  void SetDMToken() {
    // Set valid values.
    fake_dm_token_storage_.SetDMToken(kFakeDMToken);
    fake_dm_token_storage_.SetClientId(kFakeClientId);
  }

  void SetUploadResponseCode(net::HttpStatusCode status_code) {
    test_url_loader_factory_.AddResponse(kExpectedDmServerUrl, "", status_code);
  }

  void FastForwardBeyondTimeout() {
    task_environment_.FastForwardBy(kTimeoutTime + base::Seconds(2));
    task_environment_.RunUntilIdle();
  }

  absl::optional<int> RunSynchronizePublicKey(const SigningKeyPair& key_pair,
                                              bool fast_forward = false) {
    base::test::TestFuture<absl::optional<int>> future;
    launcher_->SynchronizePublicKey(key_pair, future.GetCallback());

    if (fast_forward) {
      FastForwardBeyondTimeout();
    }

    return future.Get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<testing::StrictMock<test::MockKeyRotationCommand>> mock_command_;
  ScopedKeyRotationCommandFactory scoped_command_factory_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService fake_device_management_service_{
      &job_creation_handler_};
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::unique_ptr<SigningKeyPair> test_key_pair_;
  std::unique_ptr<KeyRotationLauncher> launcher_;
};

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation) {
  SetDMToken();

  absl::optional<KeyRotationCommand::Params> params;
  EXPECT_CALL(*mock_command_, Trigger(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [&params](const KeyRotationCommand::Params given_params,
                    KeyRotationCommand::Callback callback) {
            params = given_params;
            std::move(callback).Run(KeyRotationCommand::Status::SUCCEEDED);
          }));

  launcher_->LaunchKeyRotation(kNonce, base::DoNothing());

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(kNonce, params->nonce);
  EXPECT_EQ(kFakeDMToken, params->dm_token);
  EXPECT_EQ(kExpectedDmServerUrl, params->dm_server_url);
}

TEST_F(KeyRotationLauncherTest, LaunchKeyRotation_InvalidDMToken) {
  // Set the DM token to an invalid value (i.e. empty string).
  fake_dm_token_storage_.SetDMToken("");

  bool callback_called;
  launcher_->LaunchKeyRotation(
      kNonce, base::BindLambdaForTesting(
                  [&callback_called](KeyRotationCommand::Status status) {
                    EXPECT_EQ(KeyRotationCommand::Status::FAILED, status);
                    callback_called = true;
                  }));
  EXPECT_TRUE(callback_called);
}

TEST_F(KeyRotationLauncherTest, SynchronizePublicKey_EmptyKey) {
  auto empty_key_pair = std::make_unique<SigningKeyPair>(
      nullptr, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);

  auto response_code = RunSynchronizePublicKey(*empty_key_pair);

  EXPECT_FALSE(response_code);
  histogram_tester_.ExpectUniqueSample(kSynchronizationErrorHistogram,
                                       DTSynchronizationError::kMissingKeyPair,
                                       1);
}

TEST_F(KeyRotationLauncherTest, SynchronizePublicKey_InvalidDmToken) {
  fake_dm_token_storage_.SetDMToken("");

  auto response_code = RunSynchronizePublicKey(*test_key_pair_);

  EXPECT_FALSE(response_code);
  histogram_tester_.ExpectUniqueSample(kSynchronizationErrorHistogram,
                                       DTSynchronizationError::kInvalidDmToken,
                                       1);
}

TEST_F(KeyRotationLauncherTest, SynchronizePublicKey_Success) {
  SetDMToken();

  auto expected_code = net::HTTP_OK;
  SetUploadResponseCode(expected_code);

  auto response_code = RunSynchronizePublicKey(*test_key_pair_);

  histogram_tester_.ExpectUniqueSample(kSynchronizationUploadHistogram,
                                       static_cast<int>(expected_code), 1);
  histogram_tester_.ExpectTotalCount(kSynchronizationErrorHistogram, 0);
  ASSERT_TRUE(response_code);
  EXPECT_EQ(response_code.value(), static_cast<int>(expected_code));
}

TEST_F(KeyRotationLauncherTest, SynchronizePublicKey_Timeout) {
  SetDMToken();

  auto response_code =
      RunSynchronizePublicKey(*test_key_pair_, /*fast_forward=*/true);

  // Zero is the http response code returned by the network service when hitting
  // a timeout.
  int expected_code = 0;
  histogram_tester_.ExpectUniqueSample(kSynchronizationUploadHistogram,
                                       expected_code, 1);
  histogram_tester_.ExpectTotalCount(kSynchronizationErrorHistogram, 0);
  ASSERT_TRUE(response_code);
  EXPECT_EQ(response_code.value(), expected_code);
}

}  // namespace enterprise_connectors
