// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/script_executor.h"

namespace {

class PerformanceTimelineBrowserTest : public extensions::ExtensionBrowserTest {
 protected:
  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void LoadScript(const extensions::Extension* extension) {
    std::string script_code = content::JsReplace(
        R"(
          (async () => {
            await new Promise( resolve => {
              const script = document.createElement('script');
              script.addEventListener('load', resolve);
              document.body.appendChild(script);
              script.src = $1;
            });
          })();
        )",
        extension->GetResourceURL(extension->url(), "content_script.js")
            .spec());
    EXPECT_EQ(content::EvalJs(web_contents(), script_code).error, "");
  }

  int GetActiveTabId() {
    auto* active_tab = browser()->tab_strip_model()->GetActiveWebContents();
    return sessions::SessionTabHelper::IdForTab(active_tab).id();
  }
};

}  // namespace

// Fetched resources that are initiated from the IsolatedWorld should have NO
// resource timing entry emitted.
IN_PROC_BROWSER_TEST_F(PerformanceTimelineBrowserTest,
                       ResouceTiming_IsolatedWorld) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("resource_timing/fetch_resource"));
  ASSERT_TRUE(extension);
  GURL test_url = embedded_test_server()->GetURL(
      "/extensions/resource_timing/test-page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // fetch resource from extension.
  content::DOMMessageQueue message_queue;
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('#fetchResourceButton').click();"));
  std::string ack;
  EXPECT_TRUE(message_queue.WaitForMessage(&ack));
  EXPECT_EQ("true", ack);

  // There should be 0 resource entry emitted.
  EXPECT_EQ(content::EvalJs(web_contents(), "getResourceTimingEntryCount();")
                .ExtractInt(),
            0);
}

// Fetched resources that are initiated from the MainWorld should have one
// resource timing entry emitted.
IN_PROC_BROWSER_TEST_F(PerformanceTimelineBrowserTest,
                       ResouceTiming_MainWorld) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("resource_timing/fetch_resource"));
  ASSERT_TRUE(extension);

  GURL test_url = embedded_test_server()->GetURL(
      "/extensions/resource_timing/test-page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // Add script to DOM as a script tag element.
  LoadScript(extension);

  // Execute added script which is to fetch resource;
  EXPECT_EQ(
      content::EvalJs(web_contents(), "(async ()=>{await fetchResource();})()")
          .error,
      "");

  // There should be 1 resource entry emitted.
  EXPECT_EQ(
      content::EvalJs(
          web_contents(),
          "(async ()=>{return await getResourceTimingEntryCountAsync();})()")
          .ExtractInt(),
      1);
}

// The fetchResource function is injected by the extension and executed. The csp
// world in this case is the Isolated world. No resource timing entry is
// emitted.
IN_PROC_BROWSER_TEST_F(PerformanceTimelineBrowserTest,
                       ResouceTiming_FetchWithInjectedCode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("resource_timing/fetch_resource"));
  ASSERT_TRUE(extension);

  GURL test_url = embedded_test_server()->GetURL(
      "/extensions/resource_timing/test-page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // Fetch resource by injecting fetchResource function into tab and execute.
  const int tab_id = GetActiveTabId();

  std::string script =
      R"((async () => {
           let result = await chrome.scripting.executeScript(
                   {
                     target: {tabId: $1},
                     func: fetchResource
                   });
           chrome.test.sendScriptResult(result[0].result);
         })())";

  script = content::JsReplace(script, tab_id);

  base::Value result = extensions::BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), script,
      extensions::BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

  ASSERT_TRUE(result.is_bool());
  EXPECT_TRUE(result.GetBool());

  // There should be 0 resource entry emitted.
  EXPECT_EQ(content::EvalJs(web_contents(), "getResourceTimingEntryCount();")
                .ExtractInt(),
            0);
}
