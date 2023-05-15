// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

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

  void SetUp() override {
    std::vector<uint8_t> data = GetValidPolicyFetchResponse();
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->device_account_policy = data;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    InProcessBrowserTest::SetUp();
  }

  void ReadStatusFor(const std::string& policy_legend,
                     base::flat_map<std::string, std::string>* policy_status);
};

void PolicyUiLacrosBrowserTest::ReadStatusFor(
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
          const legend = policies[i].querySelector('legend').textContent;
          const entries = {};
          const rows = policies[i]
            .querySelectorAll('.status-entry div:nth-child(2)');
          for (let j = 0; j < rows.length; ++j) {
            entries[rows[j].className] = rows[j].textContent.trim();
          }
          statuses[legend.trim()] = entries;
        }
        return JSON.stringify(statuses);
      };

      return new Promise(resolve => {
        window.requestIdleCallback(resolve);
      }).then(readStatus);
    })();
  )JS";
  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  std::string json = content::EvalJs(contents, javascript).ExtractString();
  absl::optional<base::Value> statuses = base::JSONReader::Read(json);
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
  ReadStatusFor("User policies", &status);
  EXPECT_EQ(status["managed-by"], "managed.domain");
}
