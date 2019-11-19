// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <memory>

#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

#if !defined(OS_CHROMEOS)
// HasMachineLevelPolicies() is not implemented on ChromeOS.
TEST(ChromeBrowserPolicyConnectorTest, HasMachineLevelPolicies) {
  MockConfigurationPolicyProvider provider;
  BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider);

  ChromeBrowserPolicyConnector connector;
  EXPECT_FALSE(connector.HasMachineLevelPolicies());

  PolicyMap map;
  map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
          POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>("hello"),
          nullptr);
  provider.UpdateChromePolicy(map);
  EXPECT_TRUE(connector.HasMachineLevelPolicies());
  BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace policy
