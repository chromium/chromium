// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

#if !BUILDFLAG(IS_CHROMEOS)
// HasMachineLevelPolicies() is not implemented on ChromeOS.
TEST(ChromeBrowserPolicyConnectorTest, HasMachineLevelPolicies) {
  base::test::TaskEnvironment env;
  MockConfigurationPolicyProvider provider;
  BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider);

  ChromeBrowserPolicyConnector connector;
  EXPECT_FALSE(connector.HasMachineLevelPolicies());

  PolicyMap map;
  map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_PLATFORM, base::Value("hello"), nullptr);
  provider.UpdateChromePolicy(map);
  EXPECT_TRUE(connector.HasMachineLevelPolicies());
  BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
namespace {

constexpr char kTestCachePrefName[] = "test.management_authority";

class TestManagementStatusProvider : public ManagementStatusProvider {
 public:
  TestManagementStatusProvider()
      : ManagementStatusProvider(kTestCachePrefName) {}

 protected:
  EnterpriseManagementAuthority FetchAuthority() override {
    return EnterpriseManagementAuthority::CLOUD_DOMAIN;
  }
};

}  // namespace

TEST(ChromeBrowserPolicyConnectorTest,
     RefreshManagementServiceCacheReloadsPlatformPoliciesOnChange) {
  base::test::TaskEnvironment env;
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(kTestCachePrefName,
                                        EnterpriseManagementAuthority::NONE);
  std::vector<std::unique_ptr<ManagementStatusProvider>> status_providers;
  status_providers.push_back(std::make_unique<TestManagementStatusProvider>());
  ManagementService management_service(std::move(status_providers));
  management_service.UsePrefServiceAsCache(&prefs);

  MockConfigurationPolicyProvider platform_provider;
  BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&platform_provider);
  ChromeBrowserPolicyConnector connector;

  // Nothing is cached yet, so the refresh changes the trustworthiness.
  EXPECT_CALL(platform_provider, RefreshPolicies(testing::_));
  connector.RefreshManagementServiceCache(&management_service);
  env.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&platform_provider);

  // The trustworthiness doesn't change anymore.
  EXPECT_CALL(platform_provider, RefreshPolicies(testing::_)).Times(0);
  connector.RefreshManagementServiceCache(&management_service);
  env.RunUntilIdle();

  BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace policy
