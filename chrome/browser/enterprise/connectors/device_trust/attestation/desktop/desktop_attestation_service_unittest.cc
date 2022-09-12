// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_service.h"

#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/desktop_attestation_switches.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/device_trust_key_manager_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_key_rotation_launcher.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/device_signals/core/common/signals_constants.h"
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

constexpr char kEncodedChallengeDev[] =
    "CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIK8RHA0BfjJvELuaGMIdh731PGNb/"
    "xr1iGTm7Ycs78S9GM7Yo/"
    "idMBKAAmOlxSwClQS56he7BwRdARhbqG7m6XO9YqhzssvMYKJ2uoOxdCH+FNzC8j/"
    "Kbcaq0aWoKtJUmjYJ2vJoeG0ZwMKFamHO85RRC7LvX5M3czQlJkv/"
    "wZd3KgSbMi1wDa86LWxMIJV7uBbRlkaXDGsaHGIbpqumrzX3J1f5cPRrvHQG6XHlbjBd+"
    "eXoE4tQwcHuTKc8ywPv0bmQ7kHtRhk1VRRpDcijSfp/"
    "2Q99GqWGtFS5MjCSQxwHQ2OAxr74aRYCY4mvnWLnLd02IvO9PhRa1fncT+"
    "AhOmbMq35XWmRDwPAcAf+bE23yYeur3E5V8nKulZRkVTcTbE7g3ymsrlbsCSU=";

constexpr char kDeviceId[] = "device-id";
constexpr char kObfuscatedCustomerId[] = "customer-id";

std::string GetSerializedSignedChallenge(bool use_dev) {
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

class DesktopAttestationServiceTest : public testing::TestWithParam<bool> {
 protected:
  DesktopAttestationServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    // Create the key manager and initialize it, which will make it use the
    // scoped persistence factory's default TPM-backed mock. In other words,
    // it will initialize itself with a valid key.
    key_manager_ = std::make_unique<DeviceTrustKeyManagerImpl>(
        std::make_unique<StrictMock<test::MockKeyRotationLauncher>>());
    key_manager_->StartInitialization();

    attestation_service_ =
        std::make_unique<DesktopAttestationService>(key_manager_.get());

    if (use_va_dev_keys()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kUseVaDevKeys, "");
    }
  }

  base::Value::Dict CreateSignals() {
    base::Value::Dict signals;
    signals.Set(device_signals::names::kDeviceId, kDeviceId);
    signals.Set(device_signals::names::kObfuscatedCustomerId,
                kObfuscatedCustomerId);
    return signals;
  }

  bool use_va_dev_keys() const { return GetParam(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DesktopAttestationService> attestation_service_;
  test::ScopedKeyPersistenceDelegateFactory persistence_delegate_factory_;
  std::unique_ptr<DeviceTrustKeyManagerImpl> key_manager_;
};

TEST_P(DesktopAttestationServiceTest, BuildChallengeResponse_Success) {
  // TODO(crbug.com/1208881): Add signals and validate they effectively get
  // added to the signed data.

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&](const std::string& serialized_signed_challenge) {
        ASSERT_FALSE(serialized_signed_challenge.empty());
        auto signed_data = ParseDataFromResponse(serialized_signed_challenge);
        ASSERT_TRUE(signed_data);
        EXPECT_FALSE(signed_data->data().empty());
        EXPECT_FALSE(signed_data->signature().empty());
        run_loop.Quit();
      });

  attestation_service_->BuildChallengeResponseForVAChallenge(
      GetSerializedSignedChallenge(use_va_dev_keys()), CreateSignals(),
      std::move(callback));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         DesktopAttestationServiceTest,
                         testing::Bool() /* use_va_dev_keys */);

}  // namespace enterprise_connectors
