// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash::eche_app {

namespace {

constexpr base::FilePath::CharType kTestFileLocation[] =
    FILE_PATH_LITERAL("ash/webui/eche_app_ui/test");

constexpr char kTestHarness[] = "eche_app_ui_browsertest.js";

constexpr const char* kTestFiles[] = {
    kTestHarness,
};

}  // namespace

class EcheAppUIBrowserTest : public WebUIMochaBrowserTest {
 public:
  EcheAppUIBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kEcheSWA);
  }

  void SetUpOnMainThread() override {
    SandboxedWebUiAppTestBase::ConfigureDefaultTestRequestHandler(
        base::FilePath(kTestFileLocation),
        {std::begin(kTestFiles), std::end(kTestFiles)});
    WebUIMochaBrowserTest::SetUpOnMainThread();
  }

  void RunTestCase(const std::string& test_case) {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    ASSERT_TRUE(content::NavigateToURL(
        web_contents, GURL(ash::eche_app::kChromeUIEcheAppURL)));
    ASSERT_TRUE(RunMochaTestCase(web_contents, test_case));
  }

 private:
  testing::AssertionResult RunMochaTestCase(content::WebContents* web_contents,
                                            const std::string& test_case) {
    constexpr char kLoadScript[] = R"(
        (async function() {
          await import('chrome://webui-test/mocha.js');
          await import('chrome://webui-test/mocha_adapter_simple.js');
          await import('./eche_app_ui_browsertest.js');
        })();
    )";
    testing::AssertionResult result =
        content::ExecJs(web_contents, kLoadScript);
    if (!result) {
      return result;
    }

    return RunTestOnWebContents(
        web_contents, kTestHarness,
        base::StringPrintf("runMochaTest('EcheAppUIBrowserTest', '%s')",
                           test_case.c_str()),
        /*skip_test_loader=*/false);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(EcheAppUIBrowserTest, HasChromeSchemeURL) {
  RunTestCase("HasChromeSchemeURL");
}

IN_PROC_BROWSER_TEST_F(EcheAppUIBrowserTest, GuestCanLoad) {
  RunTestCase("GuestCanLoad");
}

}  // namespace ash::eche_app
