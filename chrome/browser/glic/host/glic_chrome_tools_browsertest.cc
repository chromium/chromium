// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace glic {
namespace {

class GlicChromeToolsDisabledBrowserTest : public GlicApiBrowserTest {
 public:
  GlicChromeToolsDisabledBrowserTest()
      : GlicApiBrowserTest(
            GlicTestJsPath("./glic_chrome_tools_browsertest.js")) {}
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
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicDynamicChromeTools);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

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

// Tool execution via AiOverlayTools is currently only implemented on desktop.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicChromeToolsBrowserTest, testExecuteToolSuccess) {
  int initial_tab_count = GetTabListInterface()->GetTabCount();
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);
  EXPECT_EQ(
      GetTabListInterface()->GetActiveTab()->GetContents()->GetVisibleURL(),
      GURL("https://example.com"));
}
#endif

}  // namespace
}  // namespace glic
