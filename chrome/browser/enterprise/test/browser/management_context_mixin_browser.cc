// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/test/browser/management_context_mixin_browser.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/test/test_constants.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise::test {

namespace {

void SetProfileDMToken(Profile* profile, const std::string& dm_token) {
  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_request_token(dm_token);
  profile->GetCloudPolicyManager()
      ->core()
      ->store()
      ->set_policy_data_for_testing(std::move(policy_data));

  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken(dm_token);

  profile->GetUserCloudPolicyManager()->Connect(
      g_browser_process->local_state(), std::move(client));
}

}  // namespace

ManagementContextMixinBrowser::ManagementContextMixinBrowser(
    InProcessBrowserTestMixinHost* host,
    InProcessBrowserTest* test_base,
    ManagementContext management_context)
    : ManagementContextMixin(host, test_base, std::move(management_context)) {}

ManagementContextMixinBrowser::~ManagementContextMixinBrowser() = default;

void ManagementContextMixinBrowser::ManageCloudUser() {
  ManagementContextMixin::ManageCloudUser();
  SetProfileDMToken(browser()->profile(), kProfileDmToken);

  auto* profile_policy_manager =
      browser()->profile()->GetUserCloudPolicyManager();
  profile_policy_manager->core()->store()->set_policy_data_for_testing(
      GetBaseUserPolicyData());
}

void ManagementContextMixinBrowser::SetUpOnMainThread() {
  ManagementContextMixin::SetUpOnMainThread();

  if (management_context_.is_cloud_machine_managed) {
    auto browser_policy_data =
        std::make_unique<enterprise_management::PolicyData>();
    browser_policy_data->set_obfuscated_customer_id(kFakeCustomerId);
    browser_policy_data->add_device_affiliation_ids(kFakeCustomerId);

    auto* browser_policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    browser_policy_manager->core()->store()->set_policy_data_for_testing(
        std::move(browser_policy_data));
  }

  if (management_context_.is_cloud_user_managed) {
    ManageCloudUser();
  }
}

void ManagementContextMixinBrowser::SetUpInProcessBrowserTestFixture() {
  browser_dm_token_storage_ =
      std::make_unique<policy::FakeBrowserDMTokenStorage>();

  policy::BrowserDMTokenStorage::SetForTesting(browser_dm_token_storage_.get());
  ManagementContextMixin::SetUpInProcessBrowserTestFixture();
}

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ManagementContextMixinBrowser::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  ManagementContextMixin::SetUpDefaultCommandLine(command_line);
  command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
}
#endif

void ManagementContextMixinBrowser::ManageCloudMachine() {
  ManagementContextMixin::ManageCloudMachine();
  CHECK(browser_dm_token_storage_);
  browser_dm_token_storage_->SetEnrollmentToken(kEnrollmentToken);
  browser_dm_token_storage_->SetClientId(kBrowserClientId);
  browser_dm_token_storage_->EnableStorage(true);
  browser_dm_token_storage_->SetDMToken(kBrowserDmToken);
}

void ManagementContextMixinBrowser::SetCloudMachinePolicies(
    base::flat_map<std::string, std::optional<base::Value>> policy_entries) {
  CHECK(management_context_.is_cloud_machine_managed);
  policy::PolicyMap policy_map;

  for (auto& policy_entry : policy_entries) {
    policy_map.Set(policy_entry.first, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                   std::move(policy_entry.second), nullptr);
  }

  MergeNewChromePolicies(policy_map);
}

}  // namespace enterprise::test
