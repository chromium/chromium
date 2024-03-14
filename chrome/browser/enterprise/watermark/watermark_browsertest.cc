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

// This string checks that non-latin characters render correctly, and that extra
// whitespaces/newlines are removed.
constexpr char kWatermarkMessage[] = R"(

    THIS IS CONFIDENTIAL!

    😀😀😀 草草草 www

    مضحك جداً

)";

class WatermarkBrowserTestBase : public UiBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void ShowUi(const std::string& name) override {
    BrowserView::GetBrowserViewForBrowser(browser())->SetWatermarkString(
        kWatermarkMessage);
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
};

class WatermarkBrowserTest : public WatermarkBrowserTestBase {
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

IN_PROC_BROWSER_TEST_F(WatermarkBrowserTest, WatermarkShownAfterNavigation) {
  ShowAndVerifyUi();
}

}  // namespace enterprise_watermark
