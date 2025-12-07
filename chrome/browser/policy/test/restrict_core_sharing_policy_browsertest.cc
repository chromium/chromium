// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/values.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"

using content::RenderProcessHost;

namespace policy {

// Sets the proper policy before the browser is started.
class RestrictCoreSharingPolicyTest : public PolicyTest,
                                      public testing::WithParamInterface<bool> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // The policy is supported from Windows version 24H2.
    if (base::win::GetVersion() < base::win::Version::WIN11_24H2) {
      GTEST_SKIP();
    }

    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kRestrictCoreSharingOnRenderer, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(GetParam()), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_P(RestrictCoreSharingPolicyTest, RunTest) {
  base::Value::List renderer_processes;

  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (!host->GetProcess().IsValid()) {
      continue;
    }

    PROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY policy = {};
    EXPECT_TRUE(::GetProcessMitigationPolicy(host->GetProcess().Handle(),
                                             ProcessSideChannelIsolationPolicy,
                                             &policy, sizeof(policy)));
    if (GetParam()) {
      // If the hypervisor indicates that non-architectural core sharing is
      // unavailable because of the configured scheduler type, this mitigation
      // cannot be applied, and the OS will return ERROR_NOT_SUPPORTED.
      if (!(policy.RestrictCoreSharing)) {
        policy.RestrictCoreSharing = true;
        bool is_core_sharing_set_successful = ::SetProcessMitigationPolicy(
            ProcessSideChannelIsolationPolicy, &policy, sizeof(policy));
        EXPECT_FALSE(is_core_sharing_set_successful);
        EXPECT_TRUE(::GetLastError() == ERROR_NOT_SUPPORTED);
      }
    } else {
      EXPECT_FALSE(policy.RestrictCoreSharing);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(, RestrictCoreSharingPolicyTest, ::testing::Bool());

}  // namespace policy
