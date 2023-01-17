// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/browser/browser_device_trust_connector_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/mock_device_trust_key_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

using test::MockDeviceTrustKeyManager;

namespace {

base::Value::List GetOrigins() {
  base::Value::List origins;
  origins.Append("example1.com");
  origins.Append("example2.com");
  return origins;
}

}  // namespace

class BrowserDeviceTrustConnectorServiceTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {
    RegisterDeviceTrustConnectorProfilePrefs(profile_prefs_.registry());
    feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                       is_flag_enabled());
    UpdateAllowlistProfilePreference();
  }

  void UpdateAllowlistProfilePreference() {
    is_policy_enabled()
        ? profile_prefs_.SetManagedPref(kContextAwareAccessSignalsAllowlistPref,
                                        base::Value(GetOrigins()))
        : profile_prefs_.SetManagedPref(kContextAwareAccessSignalsAllowlistPref,
                                        base::Value(base::Value::List()));
  }

  std::unique_ptr<BrowserDeviceTrustConnectorService> CreateService() {
    auto service = std::make_unique<BrowserDeviceTrustConnectorService>(
        &mock_key_manager_, &profile_prefs_);
    service->Initialize();
    return service;
  }

  bool is_attestation_flow_enabled() {
    return is_flag_enabled() && is_policy_enabled() &&
           !has_permanent_key_creation_failure();
  }

  bool is_flag_enabled() { return std::get<0>(GetParam()); }
  bool is_policy_enabled() { return std::get<1>(GetParam()); }
  bool has_permanent_key_creation_failure() { return std::get<2>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  testing::StrictMock<MockDeviceTrustKeyManager> mock_key_manager_;
  TestingPrefServiceSimple profile_prefs_;
};

TEST_P(BrowserDeviceTrustConnectorServiceTest, IsConnectorEnabled) {
  if (is_flag_enabled() && is_policy_enabled()) {
    // Called when the service is initialized.
    EXPECT_CALL(mock_key_manager_, StartInitialization());

    // Called in `IsConnectorEnabled`.
    EXPECT_CALL(mock_key_manager_, HasPermanentFailure())
        .WillOnce(testing::Return(has_permanent_key_creation_failure()));
  }

  auto service = CreateService();
  EXPECT_EQ(is_attestation_flow_enabled(), service->IsConnectorEnabled());
}

INSTANTIATE_TEST_SUITE_P(,
                         BrowserDeviceTrustConnectorServiceTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace enterprise_connectors
