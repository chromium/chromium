// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif

namespace policy {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(ChromeBrowserPolicyConnectorTest, DeviceAffiliatedIds) {
  base::test::TaskEnvironment env;
  const char kAffiliationId[] = "affiliation-id";
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->device_properties = crosapi::mojom::DeviceProperties::New();
  init_params->device_properties->device_affiliation_ids = {kAffiliationId};
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  ChromeBrowserPolicyConnector connector;
  EXPECT_EQ(1u, connector.device_affiliation_ids().size());
  EXPECT_EQ(kAffiliationId, *connector.device_affiliation_ids().begin());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace policy
