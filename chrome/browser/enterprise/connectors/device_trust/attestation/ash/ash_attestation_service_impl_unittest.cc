// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service_impl.h"

#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key_subtle.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace enterprise_connectors {

namespace {

// A sample VerifiedAccess v2 challenge.
constexpr char kEncodedChallenge[] =
    "CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=";

constexpr char kTestUserEmail[] = "test@google.com";
constexpr char kFakeResponse[] = "fake_response";
constexpr char kDisplayName[] = "display-name";

std::string GetSerializedSignedChallenge() {
  std::string serialized_signed_challenge;
  if (!base::Base64Decode(kEncodedChallenge, &serialized_signed_challenge)) {
    return std::string();
  }

  return serialized_signed_challenge;
}

std::optional<std::string> ParseValueFromResponse(const std::string& response) {
  std::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data->GetDict().FindString("challengeResponse")) {
    return std::nullopt;
  }

  std::string decoded_response_value;
  if (!base::Base64Decode(*data->GetDict().FindString("challengeResponse"),
                          &decoded_response_value)) {
    return std::nullopt;
  }

  return decoded_response_value;
}

ash::attestation::MockTpmChallengeKeySubtle* InjectMockChallengeKeySubtle() {
  auto mock_challenge_key_subtle =
      std::make_unique<ash::attestation::MockTpmChallengeKeySubtle>();
  ash::attestation::MockTpmChallengeKeySubtle* challenge_key_subtle_ptr =
      mock_challenge_key_subtle.get();
  ash::attestation::TpmChallengeKeySubtleFactory::SetForTesting(
      std::move(mock_challenge_key_subtle));
  return challenge_key_subtle_ptr;
}

ash::attestation::MockTpmChallengeKey* InjectMockChallengeKey() {
  auto mock_challenge_key =
      std::make_unique<ash::attestation::MockTpmChallengeKey>();
  ash::attestation::MockTpmChallengeKey* challenge_key_ptr =
      mock_challenge_key.get();
  ash::attestation::TpmChallengeKeyFactory::SetForTesting(
      std::move(mock_challenge_key));
  return challenge_key_ptr;
}

}  // namespace

class AshAttestationServiceImplTest : public testing::Test {
 protected:
  AshAttestationServiceImplTest() {
    ash::AttestationClient::InitializeFake();
    mock_challenge_key_subtle_ = InjectMockChallengeKeySubtle();
    mock_challenge_key_ = InjectMockChallengeKey();
    test_profile_.set_profile_name(kTestUserEmail);
  }

  ~AshAttestationServiceImplTest() override {
    ash::attestation::TpmChallengeKeySubtleFactory::Create();
    ash::AttestationClient::Shutdown();
  }

  base::Value::Dict CreateSignals() {
    base::Value::Dict signals;
    signals.Set(device_signals::names::kDisplayName, kDisplayName);
    return signals;
  }

  void SetDeviceManagement(bool is_managed) {
    if (is_managed) {
      StubInstallAttributes()->SetCloudManaged("test_domain", "test_device_id");
    } else {
      StubInstallAttributes()->SetConsumerOwned();
    }
  }

  ash::StubInstallAttributes* StubInstallAttributes() {
    return test_profile_.ScopedCrosSettingsTestHelper()->InstallAttributes();
  }

  void VerifyAttestationFlowSuccessful(
      const AttestationResponse& attestation_response) {
    auto challenge_response = attestation_response.challenge_response;
    ASSERT_FALSE(challenge_response.empty());
    EXPECT_EQ(attestation_response.result_code, DTAttestationResult::kSuccess);
    auto parsed_value = ParseValueFromResponse(challenge_response);
    ASSERT_TRUE(parsed_value.has_value());
    EXPECT_EQ(kFakeResponse, parsed_value.value());
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile test_profile_;
  raw_ptr<ash::attestation::MockTpmChallengeKeySubtle, DanglingUntriaged>
      mock_challenge_key_subtle_;
  raw_ptr<ash::attestation::MockTpmChallengeKey, DanglingUntriaged>
      mock_challenge_key_;
  std::set<enterprise_connectors::DTCPolicyLevel> levels_;
};

TEST_F(AshAttestationServiceImplTest,
       BuildChallengeResponse_DeviceManagedSuccess) {
  SetDeviceManagement(true);

  auto attestation_service =
      std::make_unique<AshAttestationServiceImpl>(&test_profile_);

  auto protoChallenge = GetSerializedSignedChallenge();
  EXPECT_CALL(*mock_challenge_key_,
              BuildResponse(::attestation::ENTERPRISE_MACHINE,
                            /*profile=*/&test_profile_, /*callback=*/_,
                            /*challenge=*/protoChallenge,
                            /*register_key=*/false,
                            /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
                            /*key_name=*/std::string(),
                            /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          ash::attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              kFakeResponse)));

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service->BuildChallengeResponseForVAChallenge(
      protoChallenge, CreateSignals(), levels_, future.GetCallback());

  VerifyAttestationFlowSuccessful(future.Get());
}

TEST_F(AshAttestationServiceImplTest,
       BuildChallengeResponse_DeviceUnmanagedSuccess) {
  SetDeviceManagement(false);

  auto attestation_service =
      std::make_unique<AshAttestationServiceImpl>(&test_profile_);

  auto protoChallenge = GetSerializedSignedChallenge();
  EXPECT_CALL(
      *mock_challenge_key_,
      BuildResponse(
          ::attestation::DEVICE_TRUST_CONNECTOR,
          /*profile=*/&test_profile_, /*callback=*/_,
          /*challenge=*/protoChallenge,
          /*register_key=*/false,
          /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
          /*key_name=*/
          std::string(ash::attestation::kDeviceTrustConnectorKeyPrefix) +
              kTestUserEmail,
          /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          ash::attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              kFakeResponse)));

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service->BuildChallengeResponseForVAChallenge(
      protoChallenge, CreateSignals(), levels_, future.GetCallback());

  VerifyAttestationFlowSuccessful(future.Get());
}

TEST_F(AshAttestationServiceImplTest, TryPrepareKey_DeviceManagedSkip) {
  SetDeviceManagement(true);

  EXPECT_CALL(*mock_challenge_key_subtle_,
              StartPrepareKeyStep(_, _, _, _, _, _, _))
      .Times(0);

  auto attestation_service =
      std::make_unique<AshAttestationServiceImpl>(&test_profile_);
  attestation_service->TryPrepareKey();
}

TEST_F(AshAttestationServiceImplTest, TryPrepareKey_DeviceUnmanagedSuccess) {
  SetDeviceManagement(false);

  EXPECT_CALL(
      *mock_challenge_key_subtle_,
      StartPrepareKeyStep(
          ::attestation::DEVICE_TRUST_CONNECTOR, false,
          ::attestation::KEY_TYPE_RSA,
          std::string(ash::attestation::kDeviceTrustConnectorKeyPrefix) +
              kTestUserEmail,
          &test_profile_, _, _))
      .Times(1);

  auto attestation_service =
      std::make_unique<AshAttestationServiceImpl>(&test_profile_);
  attestation_service->TryPrepareKey();
}

}  // namespace enterprise_connectors
