// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/browser_attestation_service.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/attestation_switches.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace enterprise_connectors {

namespace {

class MockAttester : public Attester {
 public:
  MockAttester() {}
  ~MockAttester() override = default;

  // Attester:
  MOCK_METHOD(void,
              DecorateKeyInfo,
              (const std::set<DTCPolicyLevel>&, KeyInfo&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              SignResponse,
              (const std::set<DTCPolicyLevel>&,
               const std::string&,
               SignedData&,
               base::OnceClosure),
              (override));
};

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

constexpr char kEncodedChallengeNotFromVA[] =
    "CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS123123=";

constexpr char kEncodedChallengeDev[] =
    "CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIK8RHA0BfjJvELuaGMIdh731PGNb/"
    "xr1iGTm7Ycs78S9GM7Yo/"
    "idMBKAAmOlxSwClQS56he7BwRdARhbqG7m6XO9YqhzssvMYKJ2uoOxdCH+FNzC8j/"
    "Kbcaq0aWoKtJUmjYJ2vJoeG0ZwMKFamHO85RRC7LvX5M3czQlJkv/"
    "wZd3KgSbMi1wDa86LWxMIJV7uBbRlkaXDGsaHGIbpqumrzX3J1f5cPRrvHQG6XHlbjBd+"
    "eXoE4tQwcHuTKc8ywPv0bmQ7kHtRhk1VRRpDcijSfp/"
    "2Q99GqWGtFS5MjCSQxwHQ2OAxr74aRYCY4mvnWLnLd02IvO9PhRa1fncT+"
    "AhOmbMq35XWmRDwPAcAf+bE23yYeur3E5V8nKulZRkVTcTbE7g3ymsrlbsCSU=";

constexpr char kDisplayName[] = "display_name";

constexpr char kFakeSignature[] = "fake_signature";

std::string GetSerializedSignedChallenge(bool use_dev = false) {
  std::string serialized_signed_challenge;
  if (!base::Base64Decode(use_dev ? kEncodedChallengeDev : kEncodedChallenge,
                          &serialized_signed_challenge)) {
    return std::string();
  }

  return serialized_signed_challenge;
}

std::optional<SignedData> ParseDataFromResponse(const std::string& response) {
  std::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data->GetDict().FindString("challengeResponse")) {
    return std::nullopt;
  }

  std::string serialized_signed_challenge;
  if (!base::Base64Decode(*data->GetDict().FindString("challengeResponse"),
                          &serialized_signed_challenge)) {
    return std::nullopt;
  }

  SignedData signed_data;
  if (!signed_data.ParseFromString(serialized_signed_challenge)) {
    return std::nullopt;
  }
  return signed_data;
}

}  // namespace

class BrowserAttestationServiceTest : public testing::Test {
 protected:
  BrowserAttestationServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    auto mock_device_attester = std::make_unique<MockAttester>();
    mock_device_attester_ = mock_device_attester.get();

    auto mock_profile_attester = std::make_unique<MockAttester>();
    mock_profile_attester_ = mock_profile_attester.get();

    std::vector<std::unique_ptr<Attester>> attesters;
    attesters.push_back(std::move(mock_profile_attester));
    attesters.push_back(std::move(mock_device_attester));

    attestation_service_ =
        std::make_unique<BrowserAttestationService>(std::move(attesters));
  }

  base::Value::Dict CreateSignals() {
    base::Value::Dict signals;
    signals.Set(device_signals::names::kDisplayName, kDisplayName);
    return signals;
  }

  void MockDecorateKeyInfo() {
    EXPECT_CALL(*mock_device_attester_, DecorateKeyInfo(_, _, _))
        .WillOnce(Invoke([](const std::set<DTCPolicyLevel>& levels,
                            KeyInfo& key_info, base::OnceClosure done_closure) {
          std::move(done_closure).Run();
        }));

    EXPECT_CALL(*mock_profile_attester_, DecorateKeyInfo(_, _, _))
        .WillOnce(Invoke([](const std::set<DTCPolicyLevel>& levels,
                            KeyInfo& key_info, base::OnceClosure done_closure) {
          std::move(done_closure).Run();
        }));
  }

  void MockSignResponse(bool add_browser_signature = true) {
    EXPECT_CALL(*mock_device_attester_, SignResponse(_, _, _, _))
        .WillOnce(Invoke(
            [add_browser_signature](const std::set<DTCPolicyLevel>& levels,
                                    const std::string& challenge_response,
                                    SignedData& signed_data,
                                    base::OnceClosure done_closure) {
              ASSERT_FALSE(challenge_response.empty());
              if ((levels.find(DTCPolicyLevel::kBrowser) != levels.end()) &&
                  add_browser_signature) {
                signed_data.set_signature(kFakeSignature);
              }
              std::move(done_closure).Run();
            }));

    EXPECT_CALL(*mock_profile_attester_, SignResponse(_, _, _, _))
        .WillOnce(
            Invoke([](const std::set<DTCPolicyLevel>& levels,
                      const std::string& challenge_response,
                      SignedData& signed_data, base::OnceClosure done_closure) {
              ASSERT_FALSE(challenge_response.empty());
              std::move(done_closure).Run();
            }));
  }

  void VerifyAttestationResponse(
      const AttestationResponse& attestation_response,
      bool has_signature = true) {
    ASSERT_FALSE(attestation_response.challenge_response.empty());
    auto signed_data =
        ParseDataFromResponse(attestation_response.challenge_response);
    ASSERT_TRUE(signed_data);
    EXPECT_FALSE(signed_data->data().empty());
    EXPECT_EQ(signed_data->signature().empty(), !has_signature);

    EXPECT_EQ(attestation_response.result_code,
              has_signature ? DTAttestationResult::kSuccess
                            : DTAttestationResult::kSuccessNoSignature);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BrowserAttestationService> attestation_service_;
  raw_ptr<MockAttester> mock_device_attester_;
  raw_ptr<MockAttester> mock_profile_attester_;
};

// Test building the challenge response when the policy is enabled at both the
// user and browser-level using a Dev VA Challenge.
TEST_F(BrowserAttestationServiceTest, BuildChallengeResponseDev_Success) {
  auto levels = std::set<DTCPolicyLevel>(
      {DTCPolicyLevel::kBrowser, DTCPolicyLevel::kUser});
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseVaDevKeys, "");
  MockDecorateKeyInfo();
  MockSignResponse();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(/* use_dev= */ true), CreateSignals(),
      levels, future.GetCallback());

  VerifyAttestationResponse(future.Get());
}

// Test building the challenge response when the policy is enabled at both the
// user and browser-level using a Prod VA Challenge.
TEST_F(BrowserAttestationServiceTest, BuildChallengeResponseProd_Success) {
  auto levels = std::set<DTCPolicyLevel>(
      {DTCPolicyLevel::kBrowser, DTCPolicyLevel::kUser});
  MockDecorateKeyInfo();
  MockSignResponse();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), CreateSignals(), levels,
      future.GetCallback());

  VerifyAttestationResponse(future.Get());
}

// Test building the challenge response when the challenge is missing.
TEST_F(BrowserAttestationServiceTest, BuildChallengeResponse_EmptyChallenge) {
  auto levels = std::set<DTCPolicyLevel>(
      {DTCPolicyLevel::kBrowser, DTCPolicyLevel::kUser});

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      "", CreateSignals(), levels, future.GetCallback());

  const auto& attestation_response = future.Get();
  ASSERT_TRUE(attestation_response.challenge_response.empty());
  EXPECT_EQ(attestation_response.result_code,
            DTAttestationResult::kBadChallengeFormat);
}

// Test building the challenge response when the challenge is incorrect.
TEST_F(BrowserAttestationServiceTest,
       BuildChallengeResponse_BadChallengeSource) {
  auto levels = std::set<DTCPolicyLevel>(
      {DTCPolicyLevel::kBrowser, DTCPolicyLevel::kUser});
  std::string challenge_not_from_va;
  base::Base64Decode(kEncodedChallengeNotFromVA, &challenge_not_from_va);

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge_not_from_va, CreateSignals(), levels, future.GetCallback());

  const auto& attestation_response = future.Get();
  ASSERT_TRUE(attestation_response.challenge_response.empty());
  EXPECT_EQ(attestation_response.result_code,
            DTAttestationResult::kBadChallengeSource);
}

// Test building the challenge response when the policy is enabled at the
// browser-level only.
TEST_F(BrowserAttestationServiceTest, BuildChallengeResponse_BrowserOnly) {
  MockDecorateKeyInfo();
  MockSignResponse();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), CreateSignals(),
      std::set<DTCPolicyLevel>({DTCPolicyLevel::kBrowser}),
      future.GetCallback());

  VerifyAttestationResponse(future.Get());
}

// Test building the challenge response when the policy is enabled at the
// browser-level only and no signature is set by the device attester.
TEST_F(BrowserAttestationServiceTest,
       BuildChallengeResponse_BrowserOnly_MissingSignature) {
  MockDecorateKeyInfo();
  MockSignResponse(/*add_browser_signature=*/false);

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), CreateSignals(),
      std::set<DTCPolicyLevel>({DTCPolicyLevel::kBrowser}),
      future.GetCallback());

  VerifyAttestationResponse(future.Get(), /*has_signature=*/false);
}

// Test building the challenge response when the policy is enabled at the
// user-level only.
TEST_F(BrowserAttestationServiceTest, BuildChallengeResponse_ProfileOnly) {
  MockDecorateKeyInfo();
  MockSignResponse();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), CreateSignals(),
      std::set<DTCPolicyLevel>({DTCPolicyLevel::kUser}), future.GetCallback());

  VerifyAttestationResponse(future.Get(), /*has_signature=*/false);
}

// Test building the challenge response when the policy is not enabled at all.
TEST_F(BrowserAttestationServiceTest,
       BuildChallengeResponse_EmptyPolicyLevels) {
  MockDecorateKeyInfo();
  MockSignResponse();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), CreateSignals(),
      std::set<DTCPolicyLevel>(), future.GetCallback());

  VerifyAttestationResponse(future.Get(), /*has_signature=*/false);
}

}  // namespace enterprise_connectors
