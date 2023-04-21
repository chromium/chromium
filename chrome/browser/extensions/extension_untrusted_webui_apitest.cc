// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
  ExtensionUntrustedWebUITest() {
    scoped_feature_list_.InitWithFeatures({features::kReadAnything}, {});
  }

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

    content::RenderFrameHost* rfh = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame();
    ASSERT_TRUE(rfh);
    content::ExecuteScriptAsync(rfh, script);

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

    // Run the test.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        GURL("chrome-untrusted://read-anything-side-panel.top-chrome/")));
    content::RenderFrameHost* webui = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame();
    bool result = content::EvalJs(webui, script).ExtractBool();
    return (result) ? testing::AssertionSuccess()
                    : (testing::AssertionFailure() << "Check console output");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionUntrustedWebUITest, SanityCheckAvailableAPIs) {
  RunTestOnApiTestPage("sanity_check_available_apis.js");
}

IN_PROC_BROWSER_TEST_F(ExtensionUntrustedWebUITest,
                       SanityCheckAvailableAPIsReadAnything) {
  ASSERT_TRUE(RunTestOnReadAnythingPage(
      "sanity_check_available_apis_read_anything.js"));
}

// Tests that we can call a function that send a message to the browser and
// back.
IN_PROC_BROWSER_TEST_F(ExtensionUntrustedWebUITest, RoundTrip) {
  RunTestOnApiTestPage("round_trip.js");
}

}  // namespace extensions
