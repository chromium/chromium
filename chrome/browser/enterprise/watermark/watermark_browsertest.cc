// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

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

constexpr SkColor kTestFillColor = SkColorSetARGB(0x2A, 0, 0, 0);
constexpr SkColor kTestOutlineColor = SkColorSetARGB(0x3D, 0, 0, 0);

class WatermarkBrowserTest : public UiBrowserTest,
                             public testing::WithParamInterface<const char*> {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void NavigateToTestPage() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/enterprise/watermark/watermark_test_page.html")));
  }

  // Returns true if a watermark view object was available to set the watermark.
  bool SetWatermark(const std::string& watermark_message) {
    if (auto* watermark_view = BrowserView::GetBrowserViewForBrowser(browser())
                                   ->get_watermark_view_for_testing()) {
      watermark_view->SetString(watermark_message, kTestFillColor,
                                kTestOutlineColor);
      return true;
    }
    return false;
  }

  void ShowUi(const std::string& name) override {
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

}  // namespace

IN_PROC_BROWSER_TEST_P(WatermarkBrowserTest, WatermarkShownAfterNavigation) {
  NavigateToTestPage();
  ASSERT_TRUE(SetWatermark(GetParam()));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WatermarkBrowserTest, WatermarkClearedAfterNavigation) {
  ASSERT_TRUE(SetWatermark(GetParam()));

  // Navigating away from a watermarked page should clear the watermark if no
  // other verdict/policy is present to show a watermark.
  NavigateToTestPage();
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         WatermarkBrowserTest,
                         testing::Values(kMultilingualWatermarkMessage,
                                         kLongLinesWatermarkMessage));

class WatermarkSettingsBrowserTest : public InProcessBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  WatermarkSettingsBrowserTest() {
    if (IsCustomizationEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(kEnableWatermarkCustomization);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kEnableWatermarkCustomization);
    }
  }

  bool IsCustomizationEnabled() const { return GetParam(); }

  SkAlpha PercentageToSkAlpha(int percent_value) {
    return std::clamp(percent_value, 0, 100) * 255 / 100;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WatermarkSettingsBrowserTest, GetColors) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  // Test with default pref values.
  SkColor expected_fill_color = GetDefaultFillColor();
  SkColor expected_outline_color = GetDefaultOutlineColor();

  EXPECT_EQ(GetFillColor(prefs), expected_fill_color);
  EXPECT_EQ(GetOutlineColor(prefs), expected_outline_color);

  // Test with custom pref values.
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleFillOpacityPref, 30);
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleOutlineOpacityPref,
                    40);

  if (IsCustomizationEnabled()) {
    expected_fill_color =
        SkColorSetA(SkColorSetRGB(0x00, 0x00, 0x00), PercentageToSkAlpha(30));
    expected_outline_color =
        SkColorSetA(SkColorSetRGB(0xff, 0xff, 0xff), PercentageToSkAlpha(40));
  }

  EXPECT_EQ(GetFillColor(prefs), expected_fill_color);
  EXPECT_EQ(GetOutlineColor(prefs), expected_outline_color);
}

INSTANTIATE_TEST_SUITE_P(All, WatermarkSettingsBrowserTest, testing::Bool());
}  // namespace enterprise_watermark
