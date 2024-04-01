// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_watermark {

namespace {

// This string checks that non-latin characters render correctly.
constexpr char kMultilingualWatermarkMessage[] = R"(
    THIS IS CONFIDENTIAL!

    😀😀😀 草草草 www

    مضحك جداً
)";

// This string checks that long lines are properly handled by multiline logic.
constexpr char kLongLinesWatermarkMessage[] = R"(
This is a very long line that should be split up into multiple lines
This is a shorter line
It was not split
This is another very long line that should be split up into multiple lines
)";

class WatermarkBrowserTestBase : public UiBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void ShowUi(const std::string& name) override {
    if (auto* watermark_view = BrowserView::GetBrowserViewForBrowser(browser())
                                   ->get_watermark_view_for_testing()) {
      watermark_view->SetString(watermark_message_);
    }
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/enterprise/watermark/watermark_test_page.html")));
    base::RunLoop().RunUntilIdle();
  }

  bool VerifyUi() override {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                             ->contents_container(),
                         test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 protected:
  base::test::ScopedFeatureList scoped_features_;
  std::string watermark_message_ = kMultilingualWatermarkMessage;
};

class WatermarkBrowserTest : public WatermarkBrowserTestBase,
                             public testing::WithParamInterface<const char*> {
 public:
  WatermarkBrowserTest() {
    scoped_features_.InitAndEnableFeature(features::kEnableWatermarkView);
  }
};

class WatermarkDisabledBrowserTest : public WatermarkBrowserTestBase {
 public:
  WatermarkDisabledBrowserTest() {
    scoped_features_.InitAndDisableFeature(features::kEnableWatermarkView);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WatermarkDisabledBrowserTest,
                       NoWatermarkShownAfterNavigation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WatermarkBrowserTest, WatermarkShownAfterNavigation) {
  watermark_message_ = GetParam();
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         WatermarkBrowserTest,
                         testing::Values(kMultilingualWatermarkMessage,
                                         kLongLinesWatermarkMessage));

}  // namespace enterprise_watermark
