// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/management_context_mixin.h"

#include <utility>

#include "base/check.h"
#include "base/run_loop.h"
#include "chrome/browser/enterprise/connectors/test/test_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/enterprise/connectors/test/ash/management_context_mixin_ash.h"
#else
#include "chrome/browser/enterprise/connectors/test/browser/management_context_mixin_browser.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors::test {

// static
std::unique_ptr<ManagementContextMixin> ManagementContextMixin::Create(
    InProcessBrowserTestMixinHost* host,
    InProcessBrowserTest* test_base,
    ManagementContext management_context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<ManagementContextMixinAsh>(
      host, test_base, std::move(management_context));
#else
  return std::make_unique<ManagementContextMixinBrowser>(
      host, test_base, std::move(management_context));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ManagementContextMixin::ManagementContextMixin(
    InProcessBrowserTestMixinHost* host,
    InProcessBrowserTest* test_base,
    ManagementContext management_context)
    : InProcessBrowserTestMixin(host),
      test_base_(test_base),
      management_context_(std::move(management_context)) {}

ManagementContextMixin::~ManagementContextMixin() = default;

void ManagementContextMixin::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTestMixin::SetUpInProcessBrowserTestFixture();
  if (management_context_.is_cloud_machine_managed) {
    ManageCloudMachine();
  }

  user_policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &user_policy_provider_);
}

void ManagementContextMixin::ManageCloudUser() {
  // User is now managed. Derived classes are expected to have more logic.
  management_context_.is_cloud_user_managed = true;
}

void ManagementContextMixin::SetCloudUserPolicies(
    base::flat_map<std::string, std::optional<base::Value>> policy_entries) {
  CHECK(management_context_.is_cloud_user_managed);
  policy::PolicyMap policy_map;

  for (auto& policy_entry : policy_entries) {
    policy_map.Set(policy_entry.first, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   std::move(policy_entry.second), nullptr);
  }

  MergeNewChromePolicies(policy_map);
}

void ManagementContextMixin::ManageCloudMachine() {
  // Machine is now managed. Derived classes are expected to have more logic.
  management_context_.is_cloud_machine_managed = true;
}

std::unique_ptr<enterprise_management::PolicyData>
ManagementContextMixin::GetBaseUserPolicyData() const {
  const auto* user_customer_id =
      management_context_.affiliated ? kFakeCustomerId : kDifferentCustomerId;

  auto user_policy_data = std::make_unique<enterprise_management::PolicyData>();
  user_policy_data->set_obfuscated_customer_id(user_customer_id);
  user_policy_data->add_user_affiliation_ids(user_customer_id);
  user_policy_data->set_username(kTestUserEmail);
  user_policy_data->set_gaia_id(kTestUserId);
  return user_policy_data;
}

void ManagementContextMixin::MergeNewChromePolicies(
    policy::PolicyMap& new_policy_map) {
  // Merge the existing Chrome policies into the new map.
  new_policy_map.MergeFrom(user_policy_provider_.policies().Get(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())));

  EXPECT_NO_FATAL_FAILURE(
      user_policy_provider_.UpdateChromePolicy(new_policy_map));
  base::RunLoop().RunUntilIdle();
}

}  // namespace enterprise_connectors::test
