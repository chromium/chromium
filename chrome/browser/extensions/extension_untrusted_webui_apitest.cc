// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "extensions/test/result_catcher.h"
#include "ui/accessibility/accessibility_features.h"

namespace extensions {

namespace {

class ExtensionUntrustedWebUITest : public ExtensionApiTest {
 public:
  ExtensionUntrustedWebUITest() = default;

  ~ExtensionUntrustedWebUITest() override = default;

 protected:
  void RunTestOnApiTestPage(const char* name) {
    content::AddUntrustedDataSource(profile(), "api-test");

    std::string script;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      // Tests are located in
      // chrome/test/data/extensions/webui_untrusted/$(name).
      base::FilePath path;
      base::PathService::Get(chrome::DIR_TEST_DATA, &path);
      path = path.AppendASCII("extensions")
                 .AppendASCII("webui_untrusted")
                 .AppendASCII(name);

      // Read the test.
      ASSERT_TRUE(base::PathExists(path)) << "Couldn't find " << path.value();
      ASSERT_TRUE(base::ReadFileToString(path, &script));
      script =
          base::StrCat({"(function(){'use strict';", script.c_str(), "}())"});
    }

    // Run the test.
    ResultCatcher catcher;

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("chrome-untrusted://api-test/title1.html")));

    content::RenderFrameHost* render_frame_host = browser()
                                                      ->tab_strip_model()
                                                      ->GetActiveWebContents()
                                                      ->GetPrimaryMainFrame();
    ASSERT_TRUE(render_frame_host);
    content::ExecuteScriptAsync(render_frame_host, script);

    EXPECT_TRUE(catcher.GetNextResult());
  }

  testing::AssertionResult RunTestOnReadAnythingPage(const char* name) {
    std::string script;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      // Tests are located in
      // chrome/test/data/extensions/webui_untrusted/$(name).
      base::FilePath path;
      base::PathService::Get(chrome::DIR_TEST_DATA, &path);
      path = path.AppendASCII("extensions")
                 .AppendASCII("webui_untrusted")
                 .AppendASCII(name);

      // Read the test.
      if (!base::PathExists(path)) {
        return testing::AssertionFailure() << "Couldn't find " << path.value();
      }
      base::ReadFileToString(path, &script);
      script = "(function(){'use strict';" + script + "}());";
    }

    // Run the test. Navigating to the URL will trigger the read anything
    // navigation throttle and open the side panel instead of loading read
    // anything in the main content area.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL)));
    // Get the side panel entry registry.
    auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
    auto* side_panel_web_contents =
        side_panel_ui->GetWebContentsForTest(SidePanelEntryId::kReadAnything);

    if (!side_panel_web_contents) {
      return testing::AssertionFailure() << "Failed to navigate to WebUI";
    }
    // Wait for the view to load before trying to run the test. This ensures
    // that chrome.readingMode is set.
    content::WaitForLoadStop(side_panel_web_contents);
    // Eval the JS test.
    bool result =
        content::EvalJs(side_panel_web_contents, script).ExtractBool();
    return result ? testing::AssertionSuccess()
                  : (testing::AssertionFailure() << "Check console output");
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionUntrustedWebUITest,
                       ConfidenceCheckAvailableAPIs) {
  RunTestOnApiTestPage("confidence_check_available_apis.js");
}

IN_PROC_BROWSER_TEST_F(ExtensionUntrustedWebUITest,
                       ConfidenceCheckAvailableAPIsReadAnything) {
  ASSERT_TRUE(RunTestOnReadAnythingPage(
      "confidence_check_available_apis_read_anything.js"));
}

// Tests that we can call a function that send a message to the browser and
// back.
IN_PROC_BROWSER_TEST_F(ExtensionUntrustedWebUITest, RoundTrip) {
  RunTestOnApiTestPage("round_trip.js");
}

}  // namespace extensions
