// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace em = enterprise_management;

namespace {

std::vector<uint8_t> GetValidPolicyFetchResponse() {
  em::PolicyData policy_data;
  const em::CloudPolicySettings policy_proto;
  policy_proto.SerializeToString(policy_data.mutable_policy_value());
  policy_data.set_policy_type(policy::dm_protocol::kChromeUserPolicyType);
  policy_data.set_managed_by("managed.domain");

  em::PolicyFetchResponse policy_response;
  policy_data.SerializeToString(policy_response.mutable_policy_data());
  std::vector<uint8_t> data;
  size_t size = policy_response.ByteSizeLong();
  data.resize(size);
  policy_response.SerializeToArray(data.data(), size);
  return data;
}

}  // namespace

class PolicyUiLacrosBrowserTest : public InProcessBrowserTest {
 public:
  PolicyUiLacrosBrowserTest() = default;
  PolicyUiLacrosBrowserTest(const PolicyUiLacrosBrowserTest&) = delete;
  PolicyUiLacrosBrowserTest& operator=(const PolicyUiLacrosBrowserTest&) =
      delete;

  // Set custom init params in SetUpInProcessBrowserTestFixture, as it must
  // happen after BrowserTestBase::SetUp sets up the crosapi command line
  // switches but before the profile-independent instance of PolicyLoaderLacros
  // is initialized (ChromeMainDelegate::PostEarlyInitialization).
  void SetUpInProcessBrowserTestFixture() override {
    std::vector<uint8_t> data = GetValidPolicyFetchResponse();
    auto init_params = chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->device_account_policy = data;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void ReadStatusFor(Browser* browser,
                     const std::string& policy_legend,
                     base::flat_map<std::string, std::string>* policy_status);
};

void PolicyUiLacrosBrowserTest::ReadStatusFor(
    Browser* browser,
    const std::string& policy_legend,
    base::flat_map<std::string, std::string>* policy_status) {
  // Retrieve the text contents of the status table with specified legend.
  const std::string javascript = R"JS(
    (function() {
      function readStatus() {
        // Wait for the status box to appear in case page just loaded.
        const statusSection = document.getElementById('status-section');
        if (statusSection.hidden) {
          return new Promise(resolve => {
            window.requestIdleCallback(resolve);
          }).then(readStatus);
        }

        const policies = getPolicyFieldsets();
        const statuses = {};
        for (let i = 0; i < policies.length; ++i) {
          const statusHeading = policies[i]
            .querySelector('.status-box-heading').textContent;
          const entries = {};
          const rows = policies[i]
            .querySelectorAll('.status-entry div:nth-child(2)');
          for (let j = 0; j < rows.length; ++j) {
            entries[rows[j].className.split(' ')[0]] = rows[j].textContent
              .trim();
          }
          statuses[statusHeading.trim()] = entries;
        }
        return JSON.stringify(statuses);
      };

      return new Promise(resolve => {
        window.requestIdleCallback(resolve);
      }).then(readStatus);
    })();
  )JS";
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  std::string json = content::EvalJs(contents, javascript).ExtractString();
  std::optional<base::Value> statuses = base::JSONReader::Read(json);
  ASSERT_TRUE(statuses.has_value() && statuses->is_dict());
  const base::Value::Dict* actual_entries =
      statuses->GetDict().FindDict(policy_legend);
  ASSERT_TRUE(actual_entries);
  for (const auto entry : *actual_entries) {
    policy_status->insert_or_assign(entry.first, entry.second.GetString());
  }
}

IN_PROC_BROWSER_TEST_F(PolicyUiLacrosBrowserTest, ShowManagedByField) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIPolicyURL)));
  base::flat_map<std::string, std::string> status;
  ReadStatusFor(browser(), "User policies", &status);
  EXPECT_EQ(status["managed-by"], "managed.domain");
}

IN_PROC_BROWSER_TEST_F(PolicyUiLacrosBrowserTest,
                       ShowManagedByFieldForSecondaryProfile) {
  // Create secondary profile and a browser for it.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& secondary_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  ASSERT_FALSE(secondary_profile.IsMainProfile());
  Browser* secondary_browser = CreateBrowser(&secondary_profile);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(secondary_browser,
                                           GURL(chrome::kChromeUIPolicyURL)));
  base::flat_map<std::string, std::string> status;
  ReadStatusFor(secondary_browser, "User policies", &status);
  EXPECT_EQ(status["managed-by"], "");
}
