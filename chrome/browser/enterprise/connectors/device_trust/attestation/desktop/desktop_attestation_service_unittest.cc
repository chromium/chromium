// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Return;

namespace enterprise_connectors {

namespace {

// A sample VerifiedAccess v2 challenge rerepsented as a JSON string.
constexpr char kJsonChallenge[] =
    "{"
    "\"challenge\": "
    "\"CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=\""
    "}";

constexpr char kDeviceId[] = "device-id";
constexpr char kObfuscatedCustomerId[] = "customer-id";

absl::optional<SignedData> ParseDataFromResponse(const std::string& response) {
  absl::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data.value().FindPath("challengeResponse"))
    return absl::nullopt;

  std::string serialized_signed_data;
  if (!base::Base64Decode(
          data.value().FindPath("challengeResponse")->GetString(),
          &serialized_signed_data)) {
    return absl::nullopt;
  }

  SignedData signed_data;
  if (!signed_data.ParseFromString(serialized_signed_data)) {
    return absl::nullopt;
  }
  return signed_data;
}

enterprise_connectors::test::MockKeyPersistenceDelegate::KeyInfo
CreateEmptyKey() {
  return {enterprise_management::BrowserPublicKeyUploadRequest::
              KEY_TRUST_LEVEL_UNSPECIFIED,
          std::vector<uint8_t>()};
}

}  // namespace

class DesktopAttestationServiceTest : public testing::Test {
 protected:
  std::unique_ptr<DesktopAttestationService> CreateServiceWithLoadedKey() {
    // ScopedKeyPersistenceDelegateFactory creates mocked persistence delegates
    // that already mimic the existence of a TPM key provider and stored key.
    auto mock_persistence_delegate =
        persistence_delegate_factory_.CreateMockedDelegate();
    EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair());
    EXPECT_CALL(*mock_persistence_delegate, GetTpmBackedKeyProvider());

    auto service = std::make_unique<DesktopAttestationService>(
        std::move(mock_persistence_delegate));
    RunUntilIdle();
    return service;
  }

  std::unique_ptr<DeviceTrustSignals> CreateSignals() {
    auto signals = std::make_unique<DeviceTrustSignals>();
    signals->set_device_id(kDeviceId);
    signals->set_obfuscated_customer_id(kObfuscatedCustomerId);
    return signals;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  test::ScopedKeyPersistenceDelegateFactory* persistence_delegate_factory() {
    return &persistence_delegate_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
};

// Tests the challenge-response building flow with the signing key pair being
// available at service construction time.
TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse_InitialKeyLoaded) {
  // TODO(crbug.com/1208881): Add signals and validate they effectively get
  // added to the signed data.
  auto signals = CreateSignals();

  std::string captured_response;
  auto callback = base::BindLambdaForTesting(
      [&captured_response](const std::string& response) {
        captured_response = response;
      });

  auto attestation_service = CreateServiceWithLoadedKey();
  attestation_service->BuildChallengeResponseForVAChallenge(
      kJsonChallenge, std::move(signals), std::move(callback));
  RunUntilIdle();

  ASSERT_NE(std::string(), captured_response);

  auto signed_data = ParseDataFromResponse(captured_response);
  ASSERT_TRUE(signed_data);
  EXPECT_NE(signed_data->data(), std::string());
  EXPECT_NE(signed_data->signature(), std::string());
}

// Tests the challenge-response building flow with the signing key pair not
// being available at service construction time, but loadable during the
// attestation flow.
TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse_DelayKeyLoaded) {
  auto signals = CreateSignals();

  std::string captured_response;
  auto callback = base::BindLambdaForTesting(
      [&captured_response](const std::string& response) {
        captured_response = response;
      });

  // ScopedKeyPersistenceDelegateFactory creates mocked persistence delegates
  // that already mimic the existence of a TPM key provider and stored key.
  auto mock_persistence_delegate =
      persistence_delegate_factory()->CreateMockedDelegate();
  auto* mock_persistence_delegate_ptr = mock_persistence_delegate.get();
  EXPECT_CALL(*mock_persistence_delegate_ptr, LoadKeyPair())
      .WillOnce(Return(CreateEmptyKey()));

  auto attestation_service = std::make_unique<DesktopAttestationService>(
      std::move(mock_persistence_delegate));
  RunUntilIdle();

  // Now update the mock to expect a key actually getting loaded. The
  // persistence mock created by the factory already has some default TPM key
  // values in it, so simply expecting to get called once is good enough.
  EXPECT_CALL(*mock_persistence_delegate_ptr, LoadKeyPair());
  EXPECT_CALL(*mock_persistence_delegate_ptr, GetTpmBackedKeyProvider());

  attestation_service->BuildChallengeResponseForVAChallenge(
      kJsonChallenge, std::move(signals), std::move(callback));
  RunUntilIdle();

  ASSERT_NE(std::string(), captured_response);

  auto signed_data = ParseDataFromResponse(captured_response);
  ASSERT_TRUE(signed_data);
  EXPECT_NE(signed_data->data(), std::string());
  EXPECT_NE(signed_data->signature(), std::string());
}

// Tests the case where no key ever gets loaded, which results in the challenge
// response validation failing.
TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse_NoKey_Fails) {
  auto signals = CreateSignals();

  bool callback_called;
  std::string captured_response;
  auto callback = base::BindLambdaForTesting(
      [&callback_called, &captured_response](const std::string& response) {
        callback_called = true;
        captured_response = response;
      });

  // Mimic that no key ever gets loaded.
  auto mock_persistence_delegate =
      persistence_delegate_factory()->CreateMockedDelegate();
  EXPECT_CALL(*mock_persistence_delegate, LoadKeyPair())
      .Times(2)
      .WillRepeatedly(Return(CreateEmptyKey()));

  auto attestation_service = std::make_unique<DesktopAttestationService>(
      std::move(mock_persistence_delegate));
  RunUntilIdle();

  attestation_service->BuildChallengeResponseForVAChallenge(
      kJsonChallenge, std::move(signals), std::move(callback));
  RunUntilIdle();

  // Attestation flow failure is represented as getting an empty string as
  // challenge response.
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(std::string(), captured_response);
}

// Tests the case where a valid key is loaded but a bad challenge is given.
TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse_BadChallenge) {
  auto signals = CreateSignals();

  bool callback_called;
  std::string captured_response;
  auto callback = base::BindLambdaForTesting(
      [&callback_called, &captured_response](const std::string& response) {
        callback_called = true;
        captured_response = response;
      });

  auto attestation_service = CreateServiceWithLoadedKey();
  attestation_service->BuildChallengeResponseForVAChallenge(
      "A bad challenge for sure", std::move(signals), std::move(callback));
  RunUntilIdle();

  // Attestation flow failure is represented as getting an empty string as
  // challenge response.
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(std::string(), captured_response);
}

}  // namespace enterprise_connectors
