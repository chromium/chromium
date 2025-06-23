// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome {

class ReduceScreenSizeMetricsTest : public PlatformBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  ReduceScreenSizeMetricsTest() {
    // Enable the feature:
    feature_list_.InitWithFeatureState(
        features::kIncognitoFingerprintingInterventions, GetParam());
  }

  bool is_feature_enabled() { return GetParam(); }

  std::string viewport_dimensions(content::WebContents* web_contents) {
    gfx::Rect viewport = web_contents->GetContainerBounds();
    return base::StringPrintf("%dx%d", viewport.width(), viewport.height());
  }

  std::string actual_screen_dimensions(content::WebContents* web_contents) {
    return web_contents->GetRenderWidgetHostView()
        ->GetScreenInfo()
        .rect.size()
        .ToString();
  }

  content::WebContents* InitializeWebContents(Browser* browser) {
    // Resize the window to an invalidly-small size, which will result in the
    // window actually resizing to the smallest size allowed by the platform,
    // tab strip, scrollbars, etc. This will result something smaller than the
    // screen, which ensures a delta we can measure.
    const gfx::Rect new_bounds(10, 20, 1, 1);
    browser->window()->SetBounds(new_bounds);

    // Navigate to an empty page:
    GURL url(embedded_test_server()->GetURL("/empty.html"));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));

    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(web_contents);

    return web_contents;
  }

 protected:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ReduceScreenSizeMetricsTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return base::StringPrintf(
                               "Flag%s", info.param ? "Enabled" : "Disabled");
                         });

IN_PROC_BROWSER_TEST_P(ReduceScreenSizeMetricsTest, IncognitoScreenSize) {
  // Initialize the test with a newly-created Incognito browser:
  content::WebContents* web_contents =
      InitializeWebContents(CreateIncognitoBrowser());

  // Verify screen properties: if the feature flag is enabled, the screen size
  // will be the viewport size in Incognito browsers. If the flag is
  // disabled, the screen size will be the actual screen size.
  content::EvalJsResult reported_screen_size =
      content::EvalJs(web_contents, "`${screen.width}x${screen.height}`");
  if (is_feature_enabled()) {
    EXPECT_EQ(viewport_dimensions(web_contents), reported_screen_size);
    EXPECT_NE(actual_screen_dimensions(web_contents), reported_screen_size);
  } else {
    EXPECT_NE(viewport_dimensions(web_contents), reported_screen_size);
    EXPECT_EQ(actual_screen_dimensions(web_contents), reported_screen_size);
  }
}

IN_PROC_BROWSER_TEST_P(ReduceScreenSizeMetricsTest, RegularScreenSize) {
  // Initialize the test with the framework's existing non-Incognito browser:
  content::WebContents* web_contents = InitializeWebContents(browser());

  // Verify screen properties: regardless of the feature flag, the reported
  // screen size will be the actual screen size in non-Incognito browsers.
  std::string expectation = actual_screen_dimensions(web_contents);
  EXPECT_EQ(expectation,
            EvalJs(web_contents, "`${screen.width}x${screen.height}`"));
}

}  // namespace chrome
