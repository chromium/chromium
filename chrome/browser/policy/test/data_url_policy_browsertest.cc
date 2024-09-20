// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kGetDataURLResponse[] = R"(
  (async () => {
    function makeRequest(method, url) {
        return new Promise(function (resolve, reject) {
            let xhr = new XMLHttpRequest();
            xhr.open(method, url);
            xhr.onload = function () {
                if (this.status >= 200 && this.status < 300) {
                    resolve(xhr.responseText);
                } else {
                    reject({
                        status: this.status,
                        statusText: xhr.statusText
                    });
                }
            };
            xhr.onerror = function () {
                reject({
                    status: this.status,
                    statusText: xhr.statusText
                });
            };
            xhr.send();
        });
    }
    var result = await makeRequest("GET", "data:image/png,Hello, World!");
    return result;
  })();
)";

class DataURLPolicyTest : public PolicyTest {
 public:
  DataURLPolicyTest() = default;
  ~DataURLPolicyTest() override = default;

  // content::BrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(DataURLPolicyTest, PolicyApplies) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL url(embedded_test_server()->GetURL("a.com", "/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check the default policy off.
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("Hello, World!", EvalJs(tab, kGetDataURLResponse));

  PolicyMap policies;
  SetPolicy(&policies, key::kDataURLWhitespacePreservationEnabled,
            base::Value(false));
  UpdateProviderPolicy(policies);

  // Kill the renderer process. The policy will be set on
  // relaunch.
  content::RenderProcessHost* process =
      tab->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(-1);
  crash_observer.Wait();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("Hello,World!", EvalJs(tab, kGetDataURLResponse));
}

}  // namespace

}  // namespace policy
