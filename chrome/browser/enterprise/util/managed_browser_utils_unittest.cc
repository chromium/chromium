// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/values.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST(ManagedBrowserUtils, NoPolicies) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_FALSE(chrome::enterprise_util::HasBrowserPoliciesApplied(&profile));
}

TEST(ManagedBrowserUtils, HasManagedConnector) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(true);

  std::unique_ptr<TestingProfile> profile = builder.Build();
  EXPECT_TRUE(
      chrome::enterprise_util::HasBrowserPoliciesApplied(profile.get()));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class ManagedBrowserUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_provider_ =
        std::make_unique<policy::MockConfigurationPolicyProvider>();
    mock_provider_->Init();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(
        mock_provider_.get());
  }
  void TearDown() override {
    mock_provider_->Shutdown();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
  }
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockConfigurationPolicyProvider> mock_provider_;
};

TEST_F(ManagedBrowserUtilsTest, HasMachineLevelPolicies) {
  TestingProfile profile;

  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  mock_provider_->UpdateChromePolicy(map);

  EXPECT_TRUE(chrome::enterprise_util::HasBrowserPoliciesApplied(&profile));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
