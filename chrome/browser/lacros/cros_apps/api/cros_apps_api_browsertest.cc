// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrosAppsApiBrowserTest : public InProcessBrowserTest {
 public:
  CrosAppsApiBrowserTest() = default;
  CrosAppsApiBrowserTest(const CrosAppsApiBrowserTest&) = delete;
  CrosAppsApiBrowserTest& operator=(const CrosAppsApiBrowserTest&) = delete;
  ~CrosAppsApiBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "BlinkExtensionChromeOS");
  }
};

IN_PROC_BROWSER_TEST_F(CrosAppsApiBrowserTest, ChromeOsExistsTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "typeof window.chromeos !== 'undefined'"));
}

class DiagnosticsApiBrowserTest : public CrosAppsApiBrowserTest {
 public:
  // CrosAppsApiBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CrosAppsApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "BlinkExtensionChromeOSDiagnostics");
  }
};

IN_PROC_BROWSER_TEST_F(DiagnosticsApiBrowserTest, DiagnosticsExistsTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(
                      web_contents,
                      "typeof window.chromeos.diagnostics !== 'undefined'"));
}
