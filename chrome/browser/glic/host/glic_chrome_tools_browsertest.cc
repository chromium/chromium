// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "content/public/test/browser_test.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#endif

namespace glic {
namespace {

class GlicChromeToolsDisabledBrowserTest : public GlicApiBrowserTest {
 public:
  GlicChromeToolsDisabledBrowserTest()
      : GlicApiBrowserTest(
            GlicTestJsPath("./glic_chrome_tools_browsertest.js")) {
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicDynamicChromeTools);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicChromeToolsDisabledBrowserTest,
                       testGetChromeToolsDisabled) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicChromeToolsBrowserTest : public GlicApiBrowserTest {
 public:
  GlicChromeToolsBrowserTest()
      : GlicApiBrowserTest(
            GlicTestJsPath("./glic_chrome_tools_browsertest.js")) {
#if BUILDFLAG(IS_ANDROID)
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicDynamicChromeTools);
#endif
  }

#if BUILDFLAG(IS_ANDROID)
 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

// Dynamic Chrome tools via AiOverlayTools are only supported on non-Android
// platforms.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicChromeToolsBrowserTest, testGetChromeTools) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicChromeToolsBrowserTest, testExecuteToolNotFound) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicChromeToolsBrowserTest,
                       testExecuteToolInvalidArguments) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicChromeToolsBrowserTest, testExecuteToolSuccess) {
  int initial_tab_count = GetTabListInterface()->GetTabCount();
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);
  EXPECT_EQ(
      GetTabListInterface()->GetActiveTab()->GetContents()->GetVisibleURL(),
      GURL("https://example.com"));
}

IN_PROC_BROWSER_TEST_F(GlicChromeToolsBrowserTest,
                       testExecuteToolAfterClosingInitialBrowser) {
  ASSERT_OK(OpenGlicForActiveTabAndDetach());
  // Step 1: Execute a tool while the initial browser is active so
  // GlicToolsHolder / AiOverlayTools is instantiated before window closure.
  ExecuteJsTest();

  BrowserWindowInterface* initial_browser = GetBrowser();
  ASSERT_TRUE(initial_browser);

  // Open a second browser window for the same profile and close the initial
  // browser window while the detached Glic instance remains alive.
  BrowserWindowInterface* second_browser = CreateAdditionalBrowserWindow();
  ASSERT_TRUE(second_browser);
  TabListInterface* second_tab_list = TabListInterface::From(second_browser);
  ASSERT_TRUE(second_tab_list);
  int initial_second_tab_count = second_tab_list->GetTabCount();
  CloseBrowserSynchronously(std::exchange(initial_browser, nullptr));

  // Step 2: Execute another tool and verify it acts on the surviving browser
  // without a dangling-pointer crash.
  ContinueJsTest();
  EXPECT_EQ(second_tab_list->GetTabCount(), initial_second_tab_count + 1);
  EXPECT_EQ(second_tab_list->GetActiveTab()->GetContents()->GetVisibleURL(),
            GURL("https://example.org"));
}
#else
IN_PROC_BROWSER_TEST_F(GlicChromeToolsBrowserTest,
                       testGetChromeToolsUnsupportedOnAndroid) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}
#endif

}  // namespace
}  // namespace glic
