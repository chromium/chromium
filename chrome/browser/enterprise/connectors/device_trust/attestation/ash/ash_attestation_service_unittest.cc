// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

constexpr char kFakeResponse[] = "fake_response";
constexpr char kDisplayName[] = "display-name";

std::string GetSerializedSignedChallenge() {
  std::string serialized_signed_challenge;
  if (!base::Base64Decode(kEncodedChallenge, &serialized_signed_challenge)) {
    return std::string();
  }

  return serialized_signed_challenge;
}

absl::optional<std::string> ParseValueFromResponse(
    const std::string& response) {
  absl::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data->GetDict().FindString("challengeResponse")) {
    return absl::nullopt;
  }

  std::string decoded_response_value;
  if (!base::Base64Decode(*data->GetDict().FindString("challengeResponse"),
                          &decoded_response_value)) {
    return absl::nullopt;
  }

  return decoded_response_value;
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

class AshAttestationServiceTest : public testing::Test {
 protected:
  AshAttestationServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    ash::AttestationClient::InitializeFake();

    mock_challenge_key_ = InjectMockChallengeKey();
    attestation_service_ =
        std::make_unique<AshAttestationService>(&test_profile_);
  }

  base::Value::Dict CreateSignals() {
    base::Value::Dict signals;
    signals.Set(device_signals::names::kDisplayName, kDisplayName);
    return signals;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AshAttestationService> attestation_service_;

  TestingProfile test_profile_;
  raw_ptr<ash::attestation::MockTpmChallengeKey, ExperimentalAsh>
      mock_challenge_key_;
  std::set<enterprise_connectors::DTCPolicyLevel> levels_;
};

TEST_F(AshAttestationServiceTest, BuildChallengeResponse_Success) {
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](const AttestationResponse& attestation_response) {
        auto challenge_response = attestation_response.challenge_response;
        ASSERT_FALSE(challenge_response.empty());
        EXPECT_EQ(attestation_response.result_code,
                  DTAttestationResult::kSuccess);
        auto parsed_value = ParseValueFromResponse(challenge_response);
        ASSERT_TRUE(parsed_value.has_value());
        EXPECT_EQ(kFakeResponse, parsed_value.value());
        run_loop.Quit();
      });

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

  attestation_service_->BuildChallengeResponseForVAChallenge(
      protoChallenge, CreateSignals(), levels_, std::move(callback));
  run_loop.Run();
}

}  // namespace enterprise_connectors
