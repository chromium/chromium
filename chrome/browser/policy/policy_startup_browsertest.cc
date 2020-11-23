// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Warning: this file will not be compiled for ChromeOS because the test
// PolicyMakeDefaultBrowserTest is not valid for this platform.

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

class PolicyMakeDefaultBrowserTest : public InProcessBrowserTest {
 protected:
  PolicyMakeDefaultBrowserTest() : InProcessBrowserTest() {
    set_expected_exit_code(chrome::RESULT_CODE_ACTION_DISALLOWED_BY_POLICY);
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kMakeDefaultBrowser);
    ON_CALL(provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(policy::key::kDefaultBrowserSettingEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
    provider_.UpdateChromePolicy(values);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  DISALLOW_COPY_AND_ASSIGN(PolicyMakeDefaultBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PolicyMakeDefaultBrowserTest, MakeDefaultDisabled) {
}
