// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrosAppsApiBrowserTest : public InProcessBrowserTest {
 public:
  CrosAppsApiBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosAppsApis);
  }

  CrosAppsApiBrowserTest(const CrosAppsApiBrowserTest&) = delete;
  CrosAppsApiBrowserTest& operator=(const CrosAppsApiBrowserTest&) = delete;
  ~CrosAppsApiBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "BlinkExtensionChromeOS");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrosAppsApiBrowserTest, ChromeOsExistsTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "typeof window.chromeos !== 'undefined';"));
}

class CrosDiagnosticsApiBrowserTest : public CrosAppsApiBrowserTest {
 public:
  CrosDiagnosticsApiBrowserTest() : CrosAppsApiBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosDiagnosticsApi);
  }

  // CrosAppsApiBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CrosAppsApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "BlinkExtensionChromeOSDiagnostics");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrosDiagnosticsApiBrowserTest, DiagnosticsExistsTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(
                      web_contents,
                      "typeof window.chromeos.diagnostics !== 'undefined';"));
}

IN_PROC_BROWSER_TEST_F(CrosDiagnosticsApiBrowserTest, GetCpuInfoTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "(async () => { window.cpuInfoResult = await "
                      "window.chromeos.diagnostics.getCpuInfo(); })();"));

  {
    // Some base::SysInfo calls are blocking.
    base::ScopedAllowBlockingForTesting allow_blocking;

    EXPECT_EQ(base::SysInfo::ProcessCPUArchitecture(),
              content::EvalJs(web_contents,
                              "window.cpuInfoResult.architectureName;"));
    EXPECT_EQ(base::SysInfo::CPUModelName(),
              content::EvalJs(web_contents, "window.cpuInfoResult.modelName;"));
    EXPECT_EQ(
        base::SysInfo::NumberOfProcessors(),
        content::EvalJs(web_contents, "window.cpuInfoResult.numOfProcessors;"));
  }
}
