// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include <tuple>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/mock_attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/mock_signals_service.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NotNull;

namespace {

base::Value::List GetOrigins() {
  base::Value::List origins;
  origins.Append("example1.example.com");
  origins.Append("example2.example.com");
  return origins;
}

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

constexpr char kAttestationResponse[] =
    "{"
    "\"challengeResponse\": "
    "\"64 encoded challenge response\""
    "}";

constexpr char kResultHistogramName[] =
    "Enterprise.DeviceTrust.Attestation.Result";

std::string GetSerializedSignedChallenge(const std::string& response) {
  std::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data->GetDict().FindString("challenge")) {
    return std::string();
  }

  std::string serialized_signed_challenge;
  if (!base::Base64Decode(*data->GetDict().FindString("challenge"),
                          &serialized_signed_challenge)) {
    return std::string();
  }

  return serialized_signed_challenge;
}

}  // namespace

namespace enterprise_connectors {

using test::MockAttestationService;
using test::MockSignalsService;

class DeviceTrustServiceTest : public testing::Test,
                               public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    RegisterDeviceTrustConnectorProfilePrefs(prefs_.registry());

    levels_.insert(DTCPolicyLevel::kBrowser);

    if (is_policy_enabled()) {
      EnableServicePolicy();
    } else {
      DisableServicePolicy();
    }
  }

  void EnableServicePolicy() {
    prefs_.SetManagedPref(kBrowserContextAwareAccessSignalsAllowlistPref,
                          base::Value(GetOrigins()));
    prefs_.SetManagedPref(kUserContextAwareAccessSignalsAllowlistPref,
                          base::Value(GetOrigins()));
  }

  void DisableServicePolicy() {
    prefs_.SetManagedPref(kBrowserContextAwareAccessSignalsAllowlistPref,
                          base::Value(base::Value::List()));
    prefs_.SetManagedPref(kUserContextAwareAccessSignalsAllowlistPref,
                          base::Value(base::Value::List()));
  }

  DeviceTrustService* CreateService() {
    connector_ = std::make_unique<DeviceTrustConnectorService>(&prefs_);

    auto mock_attestation_service = std::make_unique<MockAttestationService>();
    mock_attestation_service_ = mock_attestation_service.get();

    auto mock_signals_service = std::make_unique<MockSignalsService>();
    mock_signals_service_ = mock_signals_service.get();

    device_trust_service_ = std::make_unique<DeviceTrustService>(
        std::move(mock_attestation_service), std::move(mock_signals_service),
        connector_.get());
    return device_trust_service_.get();
  }

  bool is_policy_enabled() { return GetParam(); }

  void TestFailToParseChallenge(std::string serialized_signed_challenge) {
    auto* device_trust_service = CreateService();

    EXPECT_CALL(*mock_signals_service_, CollectSignals(_)).Times(0);

    EXPECT_CALL(*mock_attestation_service_,
                BuildChallengeResponseForVAChallenge(_, _, _, _))
        .Times(0);

    base::test::TestFuture<const DeviceTrustResponse&> future;
    device_trust_service->BuildChallengeResponse(
        serialized_signed_challenge, levels_,
        /*callback=*/future.GetCallback());

    const DeviceTrustResponse& dt_response = future.Get();
    EXPECT_TRUE(dt_response.challenge_response.empty());
    EXPECT_EQ(dt_response.error, DeviceTrustError::kFailedToParseChallenge);
    EXPECT_FALSE(dt_response.attestation_result);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<DeviceTrustConnectorService> connector_;
  std::unique_ptr<DeviceTrustService> device_trust_service_;
  raw_ptr<MockAttestationService> mock_attestation_service_;
  raw_ptr<MockSignalsService> mock_signals_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  base::HistogramTester histogram_tester_;
  std::set<enterprise_connectors::DTCPolicyLevel> levels_;
};

// Tests that IsEnabled returns true only when the policy has some URLs.
TEST_P(DeviceTrustServiceTest, IsEnabled) {
  auto* device_trust_service = CreateService();
  EXPECT_EQ(is_policy_enabled(), device_trust_service->IsEnabled());
}

// Tests that the service kicks off the attestation flow properly.
TEST_P(DeviceTrustServiceTest, BuildChallengeResponse) {
  auto* device_trust_service = CreateService();

  std::string fake_display_name = "fake_display_name";
  EXPECT_CALL(*mock_signals_service_, CollectSignals(_))
      .WillOnce(Invoke(
          [&fake_display_name](
              base::OnceCallback<void(base::Value::Dict)> signals_callback) {
            auto fake_signals = std::make_unique<base::Value::Dict>();
            fake_signals->Set(device_signals::names::kDisplayName,
                              fake_display_name);
            std::move(signals_callback).Run(std::move(*fake_signals));
          }));

  const DTAttestationResult result_code = DTAttestationResult::kSuccess;
  AttestationResponse attestation_response = {kAttestationResponse,
                                              result_code};
  EXPECT_CALL(*mock_attestation_service_,
              BuildChallengeResponseForVAChallenge(
                  GetSerializedSignedChallenge(kJsonChallenge), _, levels_, _))
      .WillOnce(Invoke([&fake_display_name, &attestation_response](
                           const std::string& challenge,
                           const base::Value::Dict signals,
                           const std::set<DTCPolicyLevel> levels,
                           AttestationService::AttestationCallback callback) {
        EXPECT_EQ(
            signals.FindString(device_signals::names::kDisplayName)->c_str(),
            fake_display_name);
        std::move(callback).Run(attestation_response);
      }));

  base::test::TestFuture<const DeviceTrustResponse&> future;
  device_trust_service->BuildChallengeResponse(
      kJsonChallenge, levels_,
      /*callback=*/future.GetCallback());

  const DeviceTrustResponse& dt_response = future.Get();
  EXPECT_EQ(dt_response.challenge_response,
            attestation_response.challenge_response);
  EXPECT_FALSE(dt_response.error);
  EXPECT_EQ(dt_response.attestation_result, attestation_response.result_code);

  histogram_tester_.ExpectUniqueSample(kResultHistogramName, result_code, 1);
}

TEST_P(DeviceTrustServiceTest, AttestationFailure) {
  auto* device_trust_service = CreateService();

  std::string fake_display_name = "fake_display_name";
  EXPECT_CALL(*mock_signals_service_, CollectSignals(_))
      .WillOnce(Invoke(
          [&fake_display_name](
              base::OnceCallback<void(base::Value::Dict)> signals_callback) {
            auto fake_signals = std::make_unique<base::Value::Dict>();
            fake_signals->Set(device_signals::names::kDisplayName,
                              fake_display_name);
            std::move(signals_callback).Run(std::move(*fake_signals));
          }));

  const DTAttestationResult result_code =
      DTAttestationResult::kMissingSigningKey;
  AttestationResponse attestation_response = {kAttestationResponse,
                                              result_code};
  EXPECT_CALL(*mock_attestation_service_,
              BuildChallengeResponseForVAChallenge(
                  GetSerializedSignedChallenge(kJsonChallenge), _, levels_, _))
      .WillOnce(Invoke([&attestation_response](
                           const std::string& challenge,
                           const base::Value::Dict signals,
                           const std::set<DTCPolicyLevel> levels,
                           AttestationService::AttestationCallback callback) {
        std::move(callback).Run(attestation_response);
      }));

  base::test::TestFuture<const DeviceTrustResponse&> future;
  device_trust_service->BuildChallengeResponse(
      kJsonChallenge, levels_,
      /*callback=*/future.GetCallback());

  const DeviceTrustResponse& dt_response = future.Get();
  EXPECT_EQ(dt_response.challenge_response,
            attestation_response.challenge_response);
  EXPECT_EQ(dt_response.error, DeviceTrustError::kFailedToCreateResponse);
  EXPECT_EQ(dt_response.attestation_result, attestation_response.result_code);

  histogram_tester_.ExpectUniqueSample(kResultHistogramName, result_code, 1);
}

TEST_P(DeviceTrustServiceTest, EmptyJson) {
  TestFailToParseChallenge("");
}

TEST_P(DeviceTrustServiceTest, JsonMissingChallenge) {
  TestFailToParseChallenge("{\"not_challenge\": \"random_value\"}");
}

TEST_P(DeviceTrustServiceTest, JsonInvalidEncode) {
  TestFailToParseChallenge("{\"challenge\": \"%% %% %%\"}");
}

INSTANTIATE_TEST_SUITE_P(All, DeviceTrustServiceTest, testing::Bool());

}  // namespace enterprise_connectors
