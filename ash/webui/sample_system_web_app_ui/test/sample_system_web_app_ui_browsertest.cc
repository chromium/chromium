// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/strings/stringprintf.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

namespace {

constexpr base::FilePath::CharType kTestFileLocation[] =
    FILE_PATH_LITERAL("ash/webui/sample_system_web_app_ui/test");

constexpr char kTestHarness[] = "sample_system_web_app_ui_browsertest.js";

constexpr const char* kTestFiles[] = {
    kTestHarness,
};

}  // namespace

class SampleSystemWebAppUIBrowserTest : public WebUIMochaBrowserTest {
 public:
  SampleSystemWebAppUIBrowserTest() = default;

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
        web_contents, GURL(ash::kChromeUISampleSystemWebAppURL)));
    ASSERT_TRUE(RunMochaTestCase(web_contents,
                                 "SampleSystemWebAppUIBrowserTest", test_case));
  }

 private:
  testing::AssertionResult RunMochaTestCase(content::WebContents* web_contents,
                                            const std::string& suite_name,
                                            const std::string& test_case) {
    constexpr char kLoadScript[] = R"(
        (async function() {
          await import('chrome://webui-test/mocha.js');
          await import('chrome://webui-test/mocha_adapter_simple.js');
          await import('./sample_system_web_app_ui_browsertest.js');
        })();
    )";
    testing::AssertionResult result =
        content::ExecJs(web_contents, kLoadScript);
    if (!result) {
      return result;
    }

    return RunTestOnWebContents(
        web_contents, kTestHarness,
        base::StringPrintf("runMochaTest('%s', '%s')", suite_name.c_str(),
                           test_case.c_str()),
        /*skip_test_loader=*/false);
  }
};

// TODO(b/280457934): Skip as shared workers crash for JS coverage builds.
#if BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
#define MAYBE_HasChromeSchemeURL DISABLED_HasChromeSchemeURL
#define MAYBE_FetchPreferences DISABLED_FetchPreferences
#define MAYBE_DoSomething DISABLED_DoSomething
#else
#define MAYBE_HasChromeSchemeURL HasChromeSchemeURL
#define MAYBE_FetchPreferences FetchPreferences
#define MAYBE_DoSomething DoSomething
#endif

IN_PROC_BROWSER_TEST_F(SampleSystemWebAppUIBrowserTest,
                       MAYBE_HasChromeSchemeURL) {
  RunTestCase("HasChromeSchemeURL");
}

IN_PROC_BROWSER_TEST_F(SampleSystemWebAppUIBrowserTest,
                       MAYBE_FetchPreferences) {
  RunTestCase("FetchPreferences");
}

IN_PROC_BROWSER_TEST_F(SampleSystemWebAppUIBrowserTest, MAYBE_DoSomething) {
  RunTestCase("DoSomething");
}

class SampleSystemWebAppUIUntrustedBrowserTest : public WebUIMochaBrowserTest {
 public:
  SampleSystemWebAppUIUntrustedBrowserTest() = default;

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
        web_contents, GURL(std::string(ash::kChromeUISampleSystemWebAppURL) +
                           "/inter_frame_communication.html")));
    ASSERT_TRUE(RunMochaTestCase(
        web_contents, "SampleSystemWebAppUIUntrustedBrowserTest", test_case));
  }

 private:
  testing::AssertionResult RunMochaTestCase(content::WebContents* web_contents,
                                            const std::string& suite_name,
                                            const std::string& test_case) {
    constexpr char kLoadScript[] = R"(
        (async function() {
          await import('chrome://webui-test/mocha.js');
          await import('chrome://webui-test/mocha_adapter_simple.js');
          await import('./sample_system_web_app_ui_browsertest.js');
        })();
    )";
    testing::AssertionResult result =
        content::ExecJs(web_contents, kLoadScript);
    if (!result) {
      return result;
    }

    return RunTestOnWebContents(
        web_contents, kTestHarness,
        base::StringPrintf("runMochaTest('%s', '%s')", suite_name.c_str(),
                           test_case.c_str()),
        /*skip_test_loader=*/false);
  }
};

IN_PROC_BROWSER_TEST_F(SampleSystemWebAppUIUntrustedBrowserTest,
                       HasChromeUntrustedIframe) {
  RunTestCase("HasChromeUntrustedIframe");
}

IN_PROC_BROWSER_TEST_F(SampleSystemWebAppUIUntrustedBrowserTest,
                       MojoMethodCall) {
  RunTestCase("MojoMethodCall");
}

IN_PROC_BROWSER_TEST_F(SampleSystemWebAppUIUntrustedBrowserTest, MojoMessage) {
  RunTestCase("MojoMessage");
}

}  // namespace ash
