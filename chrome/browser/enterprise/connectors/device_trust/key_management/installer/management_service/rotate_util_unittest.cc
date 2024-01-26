// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mock_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/metrics_utils.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using enterprise_connectors::test::MockKeyNetworkDelegate;
using enterprise_connectors::test::MockKeyPersistenceDelegate;
using enterprise_connectors::test::ScopedKeyPersistenceDelegateFactory;
using HttpResponseCode =
    enterprise_connectors::test::MockKeyNetworkDelegate::HttpResponseCode;

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {

constexpr char kChromeManagementServiceStatusHistogramName[] =
    "Enterprise.DeviceTrust.ManagementService.Error";
constexpr char kNonce[] = "nonce";
constexpr char kEncodedNonce[] = "bm9uY2U=";
constexpr char kFakeDMToken[] = "fake-browser-dm-token";
constexpr char kEncodedFakeDMToken[] = "ZmFrZS1icm93c2VyLWRtLXRva2Vu";
constexpr char kFakeDmServerUrl[] =
    "https://m.google.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr char kInvalidDmServerUrl[] =
    "www.example.com/"
    "management_service?retry=false&agent=Chrome+1.2.3(456)&apptype=Chrome&"
    "critical=true&deviceid=fake-client-id&devicetype=2&platform=Test%7CUnit%"
    "7C1.2.3&request=browser_public_key_upload";
constexpr HttpResponseCode kSuccessCode = 200;
constexpr HttpResponseCode kFailureCode = 400;
constexpr HttpResponseCode kConflictCode = 409;

}  // namespace

namespace enterprise_connectors {

class RotateUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_network_delegate = std::make_unique<MockKeyNetworkDelegate>();
    auto mock_persistence_delegate = scoped_factory_.CreateMockedECDelegate();

    mock_network_delegate_ = mock_network_delegate.get();
    mock_persistence_delegate_ = mock_persistence_delegate.get();

    key_rotation_manager_ = KeyRotationManager::CreateForTesting(
        std::move(mock_network_delegate), std::move(mock_persistence_delegate));
  }

  base::CommandLine GetCommandLine(std::string token,
                                   std::string nonce,
                                   std::string url) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kRotateDTKey, token);
    command_line.AppendSwitchASCII(switches::kNonce, nonce);

    if (!url.empty())
      command_line.AppendSwitchASCII(switches::kDmServerUrl, url);

    return command_line;
  }

  raw_ptr<MockKeyNetworkDelegate, DanglingUntriaged> mock_network_delegate_;
  raw_ptr<MockKeyPersistenceDelegate, DanglingUntriaged>
      mock_persistence_delegate_;
  std::unique_ptr<KeyRotationManager> key_rotation_manager_;
  test::ScopedKeyPersistenceDelegateFactory scoped_factory_;
  base::test::TaskEnvironment task_environment_;
};

// Tests when the chrome management services key rotation was successful.
TEST_F(RotateUtilTest, RotateDTKeySuccess) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(true));

  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kSuccessCode);
      }));

  EXPECT_EQ(
      RotateDeviceTrustKey(
          std::move(key_rotation_manager_),
          GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
          version_info::Channel::STABLE),
      KeyRotationResult::kSucceeded);

  histogram_tester.ExpectTotalCount(kChromeManagementServiceStatusHistogramName,
                                    0);
}

// Tests when the chrome management services key rotation failed due to
// an invalid dm token.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidDmToken) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(RotateDeviceTrustKey(
                std::move(key_rotation_manager_),
                GetCommandLine(kFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
                version_info::Channel::STABLE),
            KeyRotationResult::kFailed);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kIncorrectlyEncodedArgument, 1);
}

// Tests when the chrome management services key rotation failed due to
// an incorrectly encoded nonce.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidNonce) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(RotateDeviceTrustKey(
                std::move(key_rotation_manager_),
                GetCommandLine(kEncodedFakeDMToken, kNonce, kFakeDmServerUrl),
                version_info::Channel::STABLE),
            KeyRotationResult::kFailed);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kIncorrectlyEncodedArgument, 1);
}

// Tests when the chrome management services key rotation failed due to
// an invalid dm server url i.e not https or http.
TEST_F(RotateUtilTest, RotateDTKeyFailure_NoDMServerUrl) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(RotateDeviceTrustKey(
                std::move(key_rotation_manager_),
                GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, ""),
                version_info::Channel::DEV),
            KeyRotationResult::kFailed);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kCommandMissingDMServerUrl, 1);
}

// Tests when the chrome management services key rotation failed due to
// a missing dm server url.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidDMServerUrl) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      RotateDeviceTrustKey(std::move(key_rotation_manager_),
                           GetCommandLine(kEncodedFakeDMToken, kEncodedNonce,
                                          kInvalidDmServerUrl),
                           version_info::Channel::DEV),
      KeyRotationResult::kFailed);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kInvalidRotateCommand, 1);
}

// Tests when the chrome management services key rotation failed due to
// an invalid rotate command i.e stable channel and non prod host name.
TEST_F(RotateUtilTest, RotateDTKeyFailure_InvalidCommand) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      RotateDeviceTrustKey(std::move(key_rotation_manager_),
                           GetCommandLine(kEncodedFakeDMToken, kEncodedNonce,
                                          kInvalidDmServerUrl),
                           version_info::Channel::STABLE),
      KeyRotationResult::kFailed);

  histogram_tester.ExpectUniqueSample(
      kChromeManagementServiceStatusHistogramName,
      ManagementServiceError::kInvalidRotateCommand, 1);
}

// Tests when the chrome management services key rotation failed due to
// incorrect signing key permissions.
TEST_F(RotateUtilTest, RotateDTKeyFailure_PermissionsFailed) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(false));

  EXPECT_EQ(
      RotateDeviceTrustKey(
          std::move(key_rotation_manager_),
          GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
          version_info::Channel::STABLE),
      KeyRotationResult::kInsufficientPermissions);

  histogram_tester.ExpectTotalCount(kChromeManagementServiceStatusHistogramName,
                                    0);
}

// Tests when the chrome management services key rotation failed due to
// an store key failure.
TEST_F(RotateUtilTest, RotateDTKeyFailure_StoreKeyFailed) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .WillOnce(Return(false));

  EXPECT_EQ(
      RotateDeviceTrustKey(
          std::move(key_rotation_manager_),
          GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
          version_info::Channel::STABLE),
      KeyRotationResult::kFailed);

  histogram_tester.ExpectTotalCount(kChromeManagementServiceStatusHistogramName,
                                    0);
}

// Tests when the chrome management services key rotation failed due to
// an upload key failure.
TEST_F(RotateUtilTest, RotateDTKeyFailure_UploadKeyFailed) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kFailureCode);
      }));

  EXPECT_EQ(
      RotateDeviceTrustKey(
          std::move(key_rotation_manager_),
          GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
          version_info::Channel::STABLE),
      KeyRotationResult::kFailed);

  histogram_tester.ExpectTotalCount(kChromeManagementServiceStatusHistogramName,
                                    0);
}

// Tests when the chrome management services key rotation failed due to
// an upload key conflict.
TEST_F(RotateUtilTest, RotateDTKeyFailure_UploadKeyConflict) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*mock_persistence_delegate_, CheckRotationPermissions())
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_persistence_delegate_, StoreKeyPair(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      *mock_network_delegate_,
      SendPublicKeyToDmServer(GURL(kFakeDmServerUrl), kFakeDMToken, _, _))
      .WillOnce(Invoke([](const GURL& url, const std::string& dm_token,
                          const std::string& body,
                          base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(kConflictCode);
      }));

  EXPECT_EQ(
      RotateDeviceTrustKey(
          std::move(key_rotation_manager_),
          GetCommandLine(kEncodedFakeDMToken, kEncodedNonce, kFakeDmServerUrl),
          version_info::Channel::STABLE),
      KeyRotationResult::kFailedKeyConflict);

  histogram_tester.ExpectTotalCount(kChromeManagementServiceStatusHistogramName,
                                    0);
}

}  // namespace enterprise_connectors
