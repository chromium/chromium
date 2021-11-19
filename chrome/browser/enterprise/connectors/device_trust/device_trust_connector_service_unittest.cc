// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kExampleDomain1[] = "example1.com";
constexpr char kExampleDomain2[] = "example2.com";
constexpr char kExampleDomain3[] = "example3.com";

constexpr char kExampleUrl1[] = "https://example1.com/somepath";
constexpr char kExampleUrl2[] = "https://example2.com/some2path";
constexpr char kExampleUrl3[] = "https://example3.com/some3path";

const base::Value origins[]{base::Value(kExampleDomain1),
                            base::Value(kExampleDomain2)};
const base::Value more_origins[]{base::Value(kExampleDomain1),
                                 base::Value(kExampleDomain2),
                                 base::Value(kExampleDomain3)};

}  // namespace

class DeviceTrustConnectorServiceTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());

    feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                       is_flag_enabled());

    if (is_policy_enabled()) {
      EnableServicePolicy();
    } else {
      DisableServicePolicy();
    }
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

  std::unique_ptr<DeviceTrustConnectorService> CreateService() {
    return std::make_unique<DeviceTrustConnectorService>(&prefs_);
  }

  bool is_attestation_flow_enabled() {
    return is_flag_enabled() && is_policy_enabled();
  }

  bool is_flag_enabled() { return std::get<0>(GetParam()); }
  bool is_policy_enabled() { return std::get<1>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
};

TEST_P(DeviceTrustConnectorServiceTest, Static_IsConnectorEnabled) {
  EXPECT_EQ(is_attestation_flow_enabled(),
            DeviceTrustConnectorService::IsConnectorEnabled(&prefs_));
}

TEST_P(DeviceTrustConnectorServiceTest, IsConnectorEnabled_Update) {
  auto service = CreateService();
  service->Initialize();
  EXPECT_EQ(is_attestation_flow_enabled(), service->IsConnectorEnabled());

  if (!is_flag_enabled()) {
    return;
  }

  UpdateServicePolicy();

  EXPECT_TRUE(service->IsConnectorEnabled());
}

TEST_P(DeviceTrustConnectorServiceTest, Matches_Update) {
  if (!is_attestation_flow_enabled()) {
    return;
  }

  auto service = CreateService();
  service->Initialize();

  GURL url1(kExampleUrl1);
  GURL url2(kExampleUrl2);
  GURL url3(kExampleUrl3);

  EXPECT_TRUE(service->Watches(url1));
  EXPECT_TRUE(service->Watches(url2));
  EXPECT_FALSE(service->Watches(url3));

  UpdateServicePolicy();

  EXPECT_TRUE(service->Watches(url1));
  EXPECT_TRUE(service->Watches(url2));
  EXPECT_TRUE(service->Watches(url3));
}

INSTANTIATE_TEST_SUITE_P(,
                         DeviceTrustConnectorServiceTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace enterprise_connectors
