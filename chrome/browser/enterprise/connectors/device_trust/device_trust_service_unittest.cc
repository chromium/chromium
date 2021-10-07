// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/barrier_closure.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/mock_attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/mock_signals_service.h"
#include "components/enterprise/common/proto/device_trust_report_event.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NotNull;

namespace {

const base::Value origins[]{base::Value("example1.example.com"),
                            base::Value("example2.example.com")};
const base::Value more_origins[]{base::Value("example1.example.com"),
                                 base::Value("example2.example.com"),
                                 base::Value("example3.example.com")};

}  // namespace

namespace enterprise_connectors {

using test::MockAttestationService;
using test::MockSignalsService;
using AttestationCallback = DeviceTrustService::AttestationCallback;

class DeviceTrustServiceTest : public testing::Test {
 protected:
  void SetUp() override { RegisterProfilePrefs(prefs_.registry()); }

  void ClearServicePolicy() {
    prefs_.RemoveUserPref(kContextAwareAccessSignalsAllowlistPref);
  }

  void EnableServicePolicy() {
    prefs_.SetUserPref(kContextAwareAccessSignalsAllowlistPref,
                       std::make_unique<base::ListValue>(origins));
  }

  void UpdateServicePolicy() {
    prefs_.SetUserPref(kContextAwareAccessSignalsAllowlistPref,
                       std::make_unique<base::ListValue>(more_origins));
  }

  void DisableServicePolicy() {
    prefs_.SetUserPref(kContextAwareAccessSignalsAllowlistPref,
                       std::make_unique<base::ListValue>());
  }

  const base::ListValue* GetPolicyUrls() {
    return prefs_.GetList(kContextAwareAccessSignalsAllowlistPref);
  }

  std::unique_ptr<DeviceTrustService> CreateService() {
    auto mock_attestation_service = std::make_unique<MockAttestationService>();
    mock_attestation_service_ = mock_attestation_service.get();

    auto mock_signals_service = std::make_unique<MockSignalsService>();
    mock_signals_service_ = mock_signals_service.get();

    return std::make_unique<DeviceTrustService>(
        &prefs_, std::move(mock_attestation_service),
        std::move(mock_signals_service));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  MockAttestationService* mock_attestation_service_;
  MockSignalsService* mock_signals_service_;
};

// Tests that IsEnabled returns the right value when the policy has some URLs.
TEST_F(DeviceTrustServiceTest, IsEnabled_True) {
  EnableServicePolicy();
  auto device_trust_service = CreateService();
  EXPECT_TRUE(device_trust_service->IsEnabled());
}

// Tests that IsEnabled returns the right value when the policy has no URLs.
TEST_F(DeviceTrustServiceTest, IsEnabled_False) {
  DisableServicePolicy();
  auto device_trust_service = CreateService();
  EXPECT_FALSE(device_trust_service->IsEnabled());
}

// Tests that callbacks get invoked when added and when the policy changes.
TEST_F(DeviceTrustServiceTest, PolicyValueCallbacks) {
  const base::ListValue* captured_policy_urls;
  auto callback = base::BindLambdaForTesting(
      [&](const base::ListValue& urls) { captured_policy_urls = &urls; });

  EnableServicePolicy();
  auto device_trust_service = CreateService();

  device_trust_service->RegisterTrustedUrlPatternsChangedCallback(callback);

  EXPECT_EQ(captured_policy_urls->GetList().size(), 2U);

  UpdateServicePolicy();

  EXPECT_EQ(captured_policy_urls->GetList().size(), 3U);
}

// Tests that the service kicks off the attestation flow properly.
TEST_F(DeviceTrustServiceTest, BuildChallengeResponse) {
  EnableServicePolicy();
  auto device_trust_service = CreateService();

  std::string fake_device_id = "fake_device_id";
  EXPECT_CALL(*mock_signals_service_, CollectSignals(_))
      .WillOnce(
          Invoke([&fake_device_id](
                     base::OnceCallback<void(std::unique_ptr<SignalsType>)>
                         signals_callback) {
            auto fake_signals = std::make_unique<SignalsType>();
            fake_signals->set_device_id(fake_device_id);
            std::move(signals_callback).Run(std::move(fake_signals));
          }));

  std::string fake_challenge = "fake_challenge";
  EXPECT_CALL(*mock_attestation_service_, BuildChallengeResponseForVAChallenge(
                                              fake_challenge, NotNull(), _))
      .WillOnce(Invoke([&fake_device_id](const std::string& challenge,
                                         std::unique_ptr<SignalsType> signals,
                                         AttestationCallback callback) {
        EXPECT_EQ(signals->device_id(), fake_device_id);
      }));

  device_trust_service->BuildChallengeResponse(
      fake_challenge,
      /*callback=*/base::BindOnce([](const std::string& response) {}));
}

}  // namespace enterprise_connectors
