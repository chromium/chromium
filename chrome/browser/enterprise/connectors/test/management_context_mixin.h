// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_MANAGEMENT_CONTEXT_MIXIN_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_MANAGEMENT_CONTEXT_MIXIN_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class ScopedDevicePolicyUpdate;
}  // namespace ash
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors::test {

struct ManagementContext {
  bool is_cloud_user_managed = false;
  bool is_cloud_machine_managed = false;
  bool affiliated = false;
};

// Utility class that can be used in browser tests to simplify Cloud management
// set-up and enabling of policies.
class ManagementContextMixin : public InProcessBrowserTestMixin {
 public:
  ManagementContextMixin(const ManagementContextMixin&) = delete;
  ManagementContextMixin& operator=(const ManagementContextMixin&) = delete;

  ~ManagementContextMixin() override;

  // Will create a mixin instance for the current platform. `host` is needed by
  // the mixin infrastructure, `test_base` allows having access to current
  // browser test instances (e.g. the Browser object), and `management_context`
  // will be used as parameter for setting up the desired user/machine cloud
  // management state.
  static std::unique_ptr<ManagementContextMixin> Create(
      InProcessBrowserTestMixinHost* host,
      InProcessBrowserTest* test_base,
      ManagementContext management_context);

  // Start managing the current cloud user.
  virtual void ManageCloudUser();

  // Will set the given `policy_entries` for the managed Cloud user. This
  // function will overwrite conflicting entries, but will not remove old policy
  // values whose keys don't conflict.
  void SetCloudUserPolicies(
      base::flat_map<std::string, std::optional<base::Value>> policy_entries);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns a scoped object which can be used to set device policies. When that
  // object goes out of scope, the policy update will be applied.
  virtual std::unique_ptr<ash::ScopedDevicePolicyUpdate>
  RequestDevicePolicyUpdate() = 0;
#else
  // Will set the given `policy_entries` for the managed Cloud browser. This
  // function will overwrite conflicting entries, but will not remove old policy
  // values whose keys don't conflict.
  virtual void SetCloudMachinePolicies(
      base::flat_map<std::string, std::optional<base::Value>>
          policy_entries) = 0;
#endif

 protected:
  ManagementContextMixin(InProcessBrowserTestMixinHost* host,
                         InProcessBrowserTest* test_base,
                         ManagementContext management_context);

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;

  virtual void ManageCloudMachine();

  // Returns a PolicyData object with some base value which can be used by
  // platform-specific mixin definitions to manage the current user.
  std::unique_ptr<enterprise_management::PolicyData> GetBaseUserPolicyData()
      const;

  // Will set policy values from `new_policy_map` into the current user's
  // PolicyBundle in a way that only overwrites existing policies when they
  // conflict.
  void MergeNewChromePolicies(policy::PolicyMap& new_policy_map);

  Browser* browser() { return test_base_->browser(); }

  const raw_ptr<InProcessBrowserTest> test_base_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      user_policy_provider_;
  ManagementContext management_context_;
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_MANAGEMENT_CONTEXT_MIXIN_H_
