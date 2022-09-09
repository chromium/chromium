// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/browser/mac_device_trust_connector_service.h"

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

class MacDeviceTrustConnectorServiceTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {
    RegisterDeviceTrustConnectorProfilePrefs(profile_prefs_.registry());
    RegisterDeviceTrustConnectorLocalPrefs(local_prefs_.registry());
    feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                       is_flag_enabled());
    UpdateAllowlistProfilePreference();
    UpdateKeyCreationLocalPreference();
  }

  void UpdateAllowlistProfilePreference() {
    is_policy_enabled()
        ? profile_prefs_.SetManagedPref(kContextAwareAccessSignalsAllowlistPref,
                                        base::Value(GetOrigins()))
        : profile_prefs_.SetManagedPref(kContextAwareAccessSignalsAllowlistPref,
                                        base::Value(base::Value::List()));
  }

  void UpdateKeyCreationLocalPreference() {
    local_prefs_.SetManagedPref(kDeviceTrustDisableKeyCreationPref,
                                base::Value(!is_key_creation_enabled()));
  }

  std::unique_ptr<MacDeviceTrustConnectorService> CreateService() {
    return std::make_unique<MacDeviceTrustConnectorService>(
        &mock_key_manager_, &profile_prefs_, &local_prefs_);
  }

  bool is_attestation_flow_enabled() {
    return is_flag_enabled() && is_policy_enabled() &&
           is_key_creation_enabled();
  }

  bool is_flag_enabled() { return std::get<0>(GetParam()); }
  bool is_policy_enabled() { return std::get<1>(GetParam()); }
  bool is_key_creation_enabled() { return !std::get<2>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  MockDeviceTrustKeyManager mock_key_manager_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_prefs_;
};

TEST_P(MacDeviceTrustConnectorServiceTest, IsConnectorEnabled) {
  auto service = CreateService();
  service->Initialize();
  EXPECT_EQ(is_attestation_flow_enabled(), service->IsConnectorEnabled());
}

// Tests that key manager is initialized only when key creation is not disabled.
TEST_P(MacDeviceTrustConnectorServiceTest, OnConnectorEnabled) {
  auto service = CreateService();
  EXPECT_CALL(mock_key_manager_, StartInitialization())
      .Times(is_key_creation_enabled() ? 1 : 0);

  service->OnConnectorEnabled();
}

INSTANTIATE_TEST_SUITE_P(,
                         MacDeviceTrustConnectorServiceTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace enterprise_connectors
