// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"

class WebSqlAccessBrowserTest : public InProcessBrowserTest {
 public:
  WebSqlAccessBrowserTest() = default;
  ~WebSqlAccessBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), content::GetTestUrl("", "simple_page.html")));
    InProcessBrowserTest::SetUpOnMainThread();
  }

  bool hasWebSQLAccess(content::WebContents* contents) {
    return EvalJs(contents, "!!window.openDatabase").ExtractBool();
  }

  bool canOpenDatabase(content::WebContents* contents) {
    return EvalJs(contents, R"(
        new Promise(function (resolve, reject) {
          try {
            window.openDatabase("test", "", "", 0);
            resolve(true);
          } catch(e) {
            resolve(false);
          }
      })
    )")
        .ExtractBool();
  }
};

IN_PROC_BROWSER_TEST_F(WebSqlAccessBrowserTest, DefaultDisabled) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(hasWebSQLAccess(web_contents));
}

// WebSQL should be accessible when blink::features::kWebSQLAccess is enabled by
// Origin Trial or chrome://flags.
class WebSqlAccessEnabledBrowserTest : public WebSqlAccessBrowserTest {
 public:
  WebSqlAccessEnabledBrowserTest() = default;
  ~WebSqlAccessEnabledBrowserTest() override = default;

  base::test::ScopedFeatureList feature_list_{blink::features::kWebSQLAccess};
};

IN_PROC_BROWSER_TEST_F(WebSqlAccessEnabledBrowserTest, FeatureFlagEnabled) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(hasWebSQLAccess(web_contents));
  EXPECT_TRUE(canOpenDatabase(web_contents));
}

// WebSQL should be accessible when switch is added by a user on the command
// line or when appended by an enterprise policy.
class WebSqlAccessCommandLineBrowserTest : public WebSqlAccessBrowserTest {
 public:
  WebSqlAccessCommandLineBrowserTest() = default;
  ~WebSqlAccessCommandLineBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(blink::switches::kWebSQLAccess);
  }
};

IN_PROC_BROWSER_TEST_F(WebSqlAccessCommandLineBrowserTest, CommandLineEnabled) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(hasWebSQLAccess(web_contents));
  EXPECT_TRUE(canOpenDatabase(web_contents));
}
