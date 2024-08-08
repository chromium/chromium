// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

#include <utility>

#include "base/values.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kExampleDomain1[] = "example1.com";
constexpr char kExampleDomain2[] = "example2.com";
constexpr char kExampleDomain3[] = "example3.com";
constexpr char kExampleDomain4[] = "example4.com";

constexpr char kExampleUrl1[] = "https://example1.com/somepath";
constexpr char kExampleUrl2[] = "https://example2.com/some2path";
constexpr char kExampleUrl3[] = "https://example3.com/some3path";
constexpr char kExampleUrl4[] = "https://example4.com/some4path";

base::Value::List GetOrigins() {
  base::Value::List origins;
  origins.Append(kExampleDomain1);
  origins.Append(kExampleDomain2);
  return origins;
}

base::Value::List GetMoreOrigins() {
  base::Value::List more_origins;
  more_origins.Append(kExampleDomain1);
  more_origins.Append(kExampleDomain2);
  more_origins.Append(kExampleDomain3);
  return more_origins;
}

base::Value::List GetDifferentOrigins() {
  base::Value::List more_origins;
  more_origins.Append(kExampleDomain3);
  more_origins.Append(kExampleDomain4);
  return more_origins;
}

void SetPolicy(TestingPrefServiceSimple* prefs,
               const std::string& pref_name,
               base::Value::List list = base::Value::List()) {
  prefs->SetManagedPref(pref_name, std::move(list));
}

class MockPolicyObserver : public DeviceTrustConnectorService::PolicyObserver {
 public:
  MockPolicyObserver() {}
  ~MockPolicyObserver() override = default;

  // DeviceTrustConnectorService::PolicyObserver:
  MOCK_METHOD(void, OnInlinePolicyEnabled, (DTCPolicyLevel), (override));
  MOCK_METHOD(void, OnInlinePolicyDisabled, (DTCPolicyLevel), (override));
};

}  // namespace

class DeviceTrustConnectorServiceTest : public testing::Test {
 protected:
  explicit DeviceTrustConnectorServiceTest(bool has_policy_value = true)
      : has_policy_value_(has_policy_value) {
    RegisterDeviceTrustConnectorProfilePrefs(prefs_.registry());

    levels_.insert(DTCPolicyLevel::kBrowser);
    levels_.insert(DTCPolicyLevel::kUser);
  }

  std::unique_ptr<DeviceTrustConnectorService> CreateService() {
    return std::make_unique<DeviceTrustConnectorService>(&prefs_);
  }

  void InitializePrefs(const std::string& pref) {
    if (has_policy_value_) {
      SetPolicy(&prefs_, pref, GetOrigins());
    }
  }

  void TestMatchesUpdateFlow(const std::string& pref,
                             const std::set<DTCPolicyLevel> level) {
    auto service = CreateService();

    GURL url1(kExampleUrl1);
    GURL url2(kExampleUrl2);
    GURL url3(kExampleUrl3);

    EXPECT_EQ(level, service->Watches(url1));
    EXPECT_EQ(level, service->Watches(url2));

    EXPECT_EQ(std::set<DTCPolicyLevel>(), service->Watches(url3));

    SetPolicy(&prefs_, pref, GetMoreOrigins());

    EXPECT_EQ(level, service->Watches(url1));
    EXPECT_EQ(level, service->Watches(url2));
    EXPECT_EQ(level, service->Watches(url3));
    EXPECT_EQ(level, service->GetEnabledInlinePolicyLevels());
  }

  void TestPolicyObserverFlow(
      const std::string& pref,
      std::set<DTCPolicyLevel> levels,
      std::set<DTCPolicyLevel> disabled_levels = std::set<DTCPolicyLevel>()) {
    auto service = CreateService();
    auto observer = std::make_unique<testing::StrictMock<MockPolicyObserver>>();
    auto* observer_ptr = observer.get();

    // The policy currently has values and adding the observer will get
    // it invoked with an "enabled" notification.
    for (const auto& level : levels) {
      EXPECT_CALL(*observer_ptr, OnInlinePolicyEnabled(level));
    }

    for (const auto& level : disabled_levels) {
      EXPECT_CALL(*observer_ptr, OnInlinePolicyDisabled(level));
    }
    service->AddObserver(std::move(observer));

    // Updating the policy to new values will trigger an "enabled"
    for (const auto& level : levels) {
      EXPECT_CALL(*observer_ptr, OnInlinePolicyEnabled(level));
    }
    SetPolicy(&prefs_, pref, GetMoreOrigins());

    // Disabling the policy will trigger a "disabled" update.
    for (const auto& level : levels) {
      EXPECT_CALL(*observer_ptr, OnInlinePolicyDisabled(level));
    }
    SetPolicy(&prefs_, pref);
  }

  TestingPrefServiceSimple prefs_;
  std::set<DTCPolicyLevel> levels_;
  bool has_policy_value_ = true;
};

// Tests that the DTC policy levels set is enabled at the correct levels for the
// UserContextAwareAccessSignalsAllowlist policy.
TEST_F(DeviceTrustConnectorServiceTest, UserPolicy_Matches_Update) {
  InitializePrefs(kUserContextAwareAccessSignalsAllowlistPref);
  TestMatchesUpdateFlow(kUserContextAwareAccessSignalsAllowlistPref,
                        std::set<DTCPolicyLevel>({DTCPolicyLevel::kUser}));
}

// Tests that the DTC policy levels set is enabled at the correct levels for the
// BrowserContextAwareAccessSignalsAllowlist policy.
TEST_F(DeviceTrustConnectorServiceTest, BrowserPolicy_Matches_Update) {
  InitializePrefs(kBrowserContextAwareAccessSignalsAllowlistPref);
  TestMatchesUpdateFlow(kBrowserContextAwareAccessSignalsAllowlistPref,
                        std::set<DTCPolicyLevel>({DTCPolicyLevel::kBrowser}));
}

// Tests that the DTC policy levels set is enabled at the correct levels when
// both the UserContextAwareAccessSignalsAllowlist and the
// BrowserContextAwareAccessSignalsAllowlist policy are enabled at the same time
// with the same policy values.
TEST_F(DeviceTrustConnectorServiceTest,
       UserAndBrowserPolicy_SameURLsMatches_Update) {
  InitializePrefs(kBrowserContextAwareAccessSignalsAllowlistPref);
  SetPolicy(&prefs_, kUserContextAwareAccessSignalsAllowlistPref, GetOrigins());

  auto service = CreateService();

  GURL url1(kExampleUrl1);
  GURL url2(kExampleUrl2);
  GURL url3(kExampleUrl3);

  EXPECT_EQ(levels_, service->Watches(url1));
  EXPECT_EQ(levels_, service->Watches(url2));
  EXPECT_EQ(std::set<DTCPolicyLevel>(), service->Watches(url3));

  // Updating the URLs.
  SetPolicy(&prefs_, kBrowserContextAwareAccessSignalsAllowlistPref,
            GetMoreOrigins());
  SetPolicy(&prefs_, kUserContextAwareAccessSignalsAllowlistPref,
            GetMoreOrigins());

  EXPECT_EQ(levels_, service->Watches(url1));
  EXPECT_EQ(levels_, service->Watches(url2));
  EXPECT_EQ(levels_, service->Watches(url3));
  EXPECT_EQ(levels_, service->GetEnabledInlinePolicyLevels());
}

// Tests that the DTC policy levels set is enabled at the correct levels when
// both the UserContextAwareAccessSignalsAllowlist and the
// BrowserContextAwareAccessSignalsAllowlist policy are enabled at the same time
// with different policy values.
TEST_F(DeviceTrustConnectorServiceTest,
       UserAndBrowserPolicy_DifferentURLsMatches_Update) {
  auto user_policy_level = std::set<DTCPolicyLevel>({DTCPolicyLevel::kUser});
  auto browser_policy_level =
      std::set<DTCPolicyLevel>({DTCPolicyLevel::kBrowser});

  InitializePrefs(kBrowserContextAwareAccessSignalsAllowlistPref);
  SetPolicy(&prefs_, kUserContextAwareAccessSignalsAllowlistPref,
            GetDifferentOrigins());

  auto service = CreateService();

  GURL url1(kExampleUrl1);
  GURL url2(kExampleUrl2);
  GURL url3(kExampleUrl3);
  GURL url4(kExampleUrl4);

  EXPECT_EQ(browser_policy_level, service->Watches(url1));
  EXPECT_EQ(browser_policy_level, service->Watches(url2));
  EXPECT_EQ(user_policy_level, service->Watches(url3));
  EXPECT_EQ(user_policy_level, service->Watches(url4));

  // Updating the URLs.
  SetPolicy(&prefs_, kBrowserContextAwareAccessSignalsAllowlistPref,
            GetDifferentOrigins());
  SetPolicy(&prefs_, kUserContextAwareAccessSignalsAllowlistPref, GetOrigins());

  EXPECT_EQ(user_policy_level, service->Watches(url1));
  EXPECT_EQ(user_policy_level, service->Watches(url2));
  EXPECT_EQ(browser_policy_level, service->Watches(url3));
  EXPECT_EQ(browser_policy_level, service->Watches(url4));
  EXPECT_EQ(levels_, service->GetEnabledInlinePolicyLevels());
}

// Tests that the policy observer behaves as intended for the
// UserContextAwareAccessSignalsAllowlist policy.
TEST_F(DeviceTrustConnectorServiceTest, UserPolicy_PolicyObserver_Notified) {
  InitializePrefs(kUserContextAwareAccessSignalsAllowlistPref);
  TestPolicyObserverFlow(
      kUserContextAwareAccessSignalsAllowlistPref,
      /*levels= */ std::set<DTCPolicyLevel>({DTCPolicyLevel::kUser}),
      /*disabled_levels= */
      std::set<DTCPolicyLevel>({DTCPolicyLevel::kBrowser}));
}

// Tests that the policy observer behaves as intended for the
// BrowserAwareAccessSignalsAllowlist policy.
TEST_F(DeviceTrustConnectorServiceTest, BrowserPolicy_PolicyObserver_Notified) {
  InitializePrefs(kBrowserContextAwareAccessSignalsAllowlistPref);
  TestPolicyObserverFlow(
      kBrowserContextAwareAccessSignalsAllowlistPref,
      /*levels= */ std::set<DTCPolicyLevel>({DTCPolicyLevel::kBrowser}),
      /*disabled_levels= */ std::set<DTCPolicyLevel>({DTCPolicyLevel::kUser}));
}

class DeviceTrustConnectorServiceFlagTest
    : public DeviceTrustConnectorServiceTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  DeviceTrustConnectorServiceFlagTest()
      : DeviceTrustConnectorServiceTest(is_policy_enabled()) {}

  bool is_policy_enabled() { return GetParam(); }

  void TestConnectorEnabledFlow(const std::string& pref) {
    auto service = CreateService();
    EXPECT_EQ(service->IsConnectorEnabled(), is_policy_enabled());

    SetPolicy(&prefs_, pref, GetOrigins());

    EXPECT_TRUE(service->IsConnectorEnabled());
  }
};

// Parameterized test covering a matrix of enabled/disabled states depending on
// both the feature flag and the policy values for the
// UserContextAwareAccessSignalsAllowlist policy.
TEST_P(DeviceTrustConnectorServiceFlagTest,
       UserPolicy_IsConnectorEnabled_Update) {
  InitializePrefs(kUserContextAwareAccessSignalsAllowlistPref);
  TestConnectorEnabledFlow(kUserContextAwareAccessSignalsAllowlistPref);
}

// Parameterized test covering a matrix of enabled/disabled states depending on
// both the feature flag and the policy values for the
// BrowserContextAwareAccessSignalsAllowlist policy.
TEST_P(DeviceTrustConnectorServiceFlagTest,
       BrowserPolicy_IsConnectorEnabled_Update) {
  InitializePrefs(kBrowserContextAwareAccessSignalsAllowlistPref);
  TestConnectorEnabledFlow(kBrowserContextAwareAccessSignalsAllowlistPref);
}

INSTANTIATE_TEST_SUITE_P(,
                         DeviceTrustConnectorServiceFlagTest,
                         testing::Bool());

}  // namespace enterprise_connectors
