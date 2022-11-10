// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_switches.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_device_trust_key_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kDisplayName[] = "display-name";
constexpr char kDmToken[] = "fake-dm-token";
constexpr char kInvalidDmToken[] = "INVALID_DM_TOKEN";

std::string GetSerializedSignedChallenge(bool use_dev = false) {
  std::string serialized_signed_challenge;
  if (!base::Base64Decode(use_dev ? kEncodedChallengeDev : kEncodedChallenge,
                          &serialized_signed_challenge)) {
    return std::string();
  }

  return serialized_signed_challenge;
}

absl::optional<SignedData> ParseDataFromResponse(const std::string& response) {
  absl::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data.value().FindPath("challengeResponse"))
    return absl::nullopt;

  std::string serialized_signed_challenge;
  if (!base::Base64Decode(
          data.value().FindPath("challengeResponse")->GetString(),
          &serialized_signed_challenge)) {
    return absl::nullopt;
  }

  SignedData signed_data;
  if (!signed_data.ParseFromString(serialized_signed_challenge)) {
    return absl::nullopt;
  }
  return signed_data;
}

}  // namespace

class DesktopAttestationServiceTest : public testing::Test {
 protected:
  DesktopAttestationServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    fake_dm_token_storage_.SetDMToken(kDmToken);
    fake_dm_token_storage_.SetClientId(kFakeDeviceId);
    auto* factory = KeyPersistenceDelegateFactory::GetInstance();
    DCHECK(factory);
    test_key_pair_ = factory->CreateKeyPersistenceDelegate()->LoadKeyPair();

    mock_key_manager_ = std::make_unique<test::MockDeviceTrustKeyManager>();

    attestation_service_ = std::make_unique<DesktopAttestationService>(
        &fake_dm_token_storage_, mock_key_manager_.get());
  }

  void SetupPubkeyExport(bool can_export_pubkey = true) {
    EXPECT_CALL(*mock_key_manager_, ExportPublicKeyAsync(_))
        .WillOnce(
            Invoke([&, can_export_pubkey](
                       base::OnceCallback<void(absl::optional<std::string>)>
                           callback) {
              if (can_export_pubkey) {
                auto public_key_info =
                    test_key_pair_->key()->GetSubjectPublicKeyInfo();
                std::string public_key(public_key_info.begin(),
                                       public_key_info.end());
                std::move(callback).Run(public_key);
              } else {
                std::move(callback).Run(absl::nullopt);
              }
            }));
  }

  void SetupSignature(bool can_sign = true) {
    EXPECT_CALL(*mock_key_manager_, SignStringAsync(_, _))
        .WillOnce(Invoke(
            [&, can_sign](const std::string& str,
                          base::OnceCallback<void(
                              absl::optional<std::vector<uint8_t>>)> callback) {
              if (can_sign) {
                std::move(callback).Run(test_key_pair_->key()->SignSlowly(
                    base::as_bytes(base::make_span(str))));
              } else {
                std::move(callback).Run(absl::nullopt);
              }
            }));
  }

  base::Value::Dict CreateSignals() {
    base::Value::Dict signals;
    signals.Set(device_signals::names::kDisplayName, kDisplayName);
    return signals;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SigningKeyPair> test_key_pair_;
  std::unique_ptr<DesktopAttestationService> attestation_service_;
  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
  std::unique_ptr<test::MockDeviceTrustKeyManager> mock_key_manager_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
};

TEST_F(DesktopAttestationServiceTest, BuildChallengeResponseDev_Success) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseVaDevKeys, "");

  SetupPubkeyExport();
  SetupSignature();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(/* use_dev= */ true), CreateSignals(),
      future.GetCallback());
  const auto& attestation_response = future.Get();
  ASSERT_FALSE(attestation_response.challenge_response.empty());
  auto signed_data =
      ParseDataFromResponse(attestation_response.challenge_response);
  ASSERT_TRUE(signed_data);
  EXPECT_FALSE(signed_data->data().empty());
  EXPECT_FALSE(signed_data->signature().empty());

  EXPECT_EQ(attestation_response.result_code, DTAttestationResult::kSuccess);
}

TEST_F(DesktopAttestationServiceTest, BuildChallengeResponseProd_Success) {
  SetupPubkeyExport();
  SetupSignature();

  // TODO(crbug.com/1208881): Add signals and validate they effectively get
  // added to the signed data.
  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(/* use_dev= */ false), CreateSignals(),
      future.GetCallback());
  const auto& attestation_response = future.Get();
  ASSERT_FALSE(attestation_response.challenge_response.empty());
  auto signed_data =
      ParseDataFromResponse(attestation_response.challenge_response);
  ASSERT_TRUE(signed_data);
  EXPECT_FALSE(signed_data->data().empty());
  EXPECT_FALSE(signed_data->signature().empty());

  EXPECT_EQ(attestation_response.result_code, DTAttestationResult::kSuccess);
}

TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse_InvalidDmToken) {
  fake_dm_token_storage_.SetDMToken(kInvalidDmToken);

  SetupPubkeyExport();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), CreateSignals(), future.GetCallback());

  const auto& attestation_response = future.Get();
  // No challenge response is returned if no valid DMToken was found.
  ASSERT_TRUE(attestation_response.challenge_response.empty());
  EXPECT_EQ(attestation_response.result_code,
            DTAttestationResult::kMissingCoreSignals);
}

TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse_EmptyDmToken) {
  fake_dm_token_storage_.SetDMToken(std::string());

  SetupPubkeyExport();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(), CreateSignals(), future.GetCallback());

  const auto& attestation_response = future.Get();
  // No challenge response is returned if no valid DMToken was found.
  ASSERT_TRUE(attestation_response.challenge_response.empty());
  EXPECT_EQ(attestation_response.result_code,
            DTAttestationResult::kMissingCoreSignals);
}

TEST_F(DesktopAttestationServiceTest,
       BuildChallengeResponse_MissingSigningKey) {
  SetupPubkeyExport(/*can_export_pubkey=*/false);

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(/* use_dev= */ false), CreateSignals(),
      future.GetCallback());

  const auto& response = future.Get();
  ASSERT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.result_code, DTAttestationResult::kMissingSigningKey);
}

TEST_F(DesktopAttestationServiceTest, BuildChallengeResponse_EmptyChallenge) {
  SetupPubkeyExport();

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      "", CreateSignals(), future.GetCallback());

  const auto& attestation_response = future.Get();
  ASSERT_TRUE(attestation_response.challenge_response.empty());
  EXPECT_EQ(attestation_response.result_code,
            DTAttestationResult::kBadChallengeFormat);
}

TEST_F(DesktopAttestationServiceTest,
       BuildChallengeResponse_BadChallengeSource) {
  SetupPubkeyExport();

  std::string challenge_not_from_va;
  base::Base64Decode(kEncodedChallengeNotFromVA, &challenge_not_from_va);

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge_not_from_va, CreateSignals(), future.GetCallback());

  const auto& attestation_response = future.Get();
  ASSERT_TRUE(attestation_response.challenge_response.empty());
  EXPECT_EQ(attestation_response.result_code,
            DTAttestationResult::kBadChallengeSource);
}

TEST_F(DesktopAttestationServiceTest,
       BuildChallengeResponse_EmptyEncryptedResponse) {
  SetupPubkeyExport();
  SetupSignature(/*can_sign=*/false);

  base::test::TestFuture<const AttestationResponse&> future;
  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(/* use_dev= */ false), CreateSignals(),
      future.GetCallback());

  const auto& response = future.Get();
  ASSERT_TRUE(response.challenge_response.empty());
  EXPECT_EQ(response.result_code, DTAttestationResult::kFailedToSignResponse);
}

// TODO(crbug.com/1208881): Add signals and validate they effectively get
// added to the signed data in new tests.

}  // namespace enterprise_connectors
