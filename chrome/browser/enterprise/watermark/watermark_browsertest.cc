// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/watermark/watermark_page_handler.h"
#include "chrome/browser/ui/webui/watermark/watermark_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

struct WatermarkTextParams {
  const char* test_suffix;
  const char* watermark_text;
};

struct WatermarkStyleParams {
  const char* test_suffix;
  int fill_opacity;
  int outline_opacity;
  int font_size;
};

constexpr SkColor kTestFillColor = SkColorSetARGB(0x2A, 0, 0, 0);
constexpr SkColor kTestOutlineColor = SkColorSetARGB(0x3D, 0, 0, 0);
constexpr int kTestFontSize =
    enterprise_connectors::kWatermarkStyleFontSizeDefault;

class WatermarkBrowserTest
    : public UiBrowserTest,
      public testing::WithParamInterface<WatermarkTextParams> {
 public:
  WatermarkBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        enterprise_data_protection::kEnableSinglePageAppDataProtection);
  }

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
                                   ->GetContentsContainerViews()[0]
                                   ->watermark_view()) {
      watermark_view->SetString(watermark_message, kTestFillColor,
                                kTestOutlineColor, kTestFontSize);
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

    // Use the test suffix to create a unique name for the golden image.
    const std::string name =
        std::string(test_info->name()) + "_" + GetParam().test_suffix;

    return VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                             ->contents_container(),
                         test_info->test_suite_name(),
                         name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(WatermarkBrowserTest, WatermarkShownAfterNavigation) {
  NavigateToTestPage();
  ASSERT_TRUE(SetWatermark(GetParam().watermark_text));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WatermarkBrowserTest, WatermarkClearedAfterNavigation) {
  ASSERT_TRUE(SetWatermark(GetParam().watermark_text));

  // Navigating away from a watermarked page should clear the watermark if no
  // other verdict/policy is present to show a watermark.
  NavigateToTestPage();
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WatermarkBrowserTest,
    testing::Values(
        WatermarkTextParams{"Multilingual", kMultilingualWatermarkMessage},
        WatermarkTextParams{"LongLines", kLongLinesWatermarkMessage}));

// Test fixture for the default chrome://watermark page.
class WatermarkTestPageBrowserTest : public UiBrowserTest {
 public:
  WatermarkTestPageBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kEnableWatermarkTestPage);
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {

class FakeRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  FakeRealTimeUrlLookupService() = default;

  // RealTimeUrlLookupServiceBase:
  void StartMaybeCachedLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id,
      std::optional<safe_browsing::internal::ReferringAppInfo>
          referring_app_info,
      bool use_cache) override {
    auto response = std::make_unique<safe_browsing::RTLookupResponse>();
    safe_browsing::RTLookupResponse::ThreatInfo* new_threat_info =
        response->add_threat_info();
    safe_browsing::MatchedUrlNavigationRule* matched_url_navigation_rule =
        new_threat_info->mutable_matched_url_navigation_rule();

    // Only add a watermark for watermark.com URLs.
    if (url.GetHost() == "watermark.com") {
      safe_browsing::MatchedUrlNavigationRule::WatermarkMessage wm;
      wm.set_watermark_message("custom_messge");
      wm.mutable_timestamp()->set_seconds(base::Time::Now().ToTimeT());
      *matched_url_navigation_rule->mutable_watermark_message() = wm;
    }

    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(response_callback),
                       /*is_rt_lookup_successful=*/true,
                       /*is_cached_response=*/true, std::move(response)));
  }
};

}  // namespace

class WatermarkBrowserNavigationTest : public InProcessBrowserTest {
 public:
  WatermarkBrowserNavigationTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kSideBySide);
  }
  WatermarkBrowserNavigationTest(const WatermarkBrowserNavigationTest&) =
      delete;
  WatermarkBrowserNavigationTest& operator=(
      const WatermarkBrowserNavigationTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set a DM token since the enterprise real-time URL service expects one.
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

    auto create_service_callback =
        base::BindRepeating([](content::BrowserContext* context) {
          Profile* profile = Profile::FromBrowserContext(context);

          // Enable real-time URL checks.
          profile->GetPrefs()->SetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
              enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
          profile->GetPrefs()->SetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
              policy::POLICY_SCOPE_MACHINE);

          auto testing_factory =
              base::BindRepeating([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
                return std::make_unique<FakeRealTimeUrlLookupService>();
              });
          safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
              GetInstance()
                  ->SetTestingFactory(context, testing_factory);
        });

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(create_service_callback);
  }

  content::WebContents* NavigateAsync(const GURL& url) {
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
    Navigate(&params);
    return params.navigated_or_inserted_contents;
  }

  void NavigateToAndWait(const GURL& url) {
    content::WaitForLoadStop(NavigateAsync(url));
  }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  base::CallbackListSubscription create_services_subscription_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WatermarkBrowserNavigationTest, Apply_NoWatermark) {
  NavigateToAndWait(GURL("https://nowatermark.com"));
  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());
}

IN_PROC_BROWSER_TEST_F(WatermarkBrowserNavigationTest,
                       Apply_Nav_NoWatermark_Watermark) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Initial page loaded into the browser view is a chrome:// URL that has no
  // watermark.
  EXPECT_FALSE(browser_view->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());

  base::test::TestFuture<void> future;
  browser()
      ->GetActiveTabInterface()
      ->GetTabFeatures()
      ->data_protection_controller()
      ->SetCallbackForTesting(future.GetCallback());
  // Navigate to a page that should show a watermark.  The watermark should
  // show even while the page loads.
  auto* web_contents = NavigateAsync(GURL("https://watermark.com"));
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(browser_view->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  // Once the page loads, the watermark should remain.
  content::WaitForLoadStop(web_contents);
  EXPECT_TRUE(browser_view->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());
}

IN_PROC_BROWSER_TEST_F(WatermarkBrowserNavigationTest,
                       Apply_Nav_Watermark_NoWatermark) {
  // Start on a page that should show a watermark.
  NavigateToAndWait(GURL("https://watermark.com"));
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  // Navigate to a page that should not show a watermark.  The watermark should
  // still show while the page loads.
  auto* web_contents = NavigateAsync(GURL("https://nowatermark.com"));
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  // Once the page loads, the watermark should be cleared.
  content::WaitForLoadStop(web_contents);
  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());
}

IN_PROC_BROWSER_TEST_F(WatermarkBrowserNavigationTest,
                       Apply_SwitchTab_ToWatermark) {
  NavigateToAndWait(GURL("https://watermark.com"));

  // Create a second tab with a page that should not be watermarked.
  // AddTabAtIndex() waits for the load to finish and activates the tab.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://version"), ui::PAGE_TRANSITION_LINK));
  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());

  // Switch active tabs back to watermarked page.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());
}

IN_PROC_BROWSER_TEST_F(WatermarkBrowserNavigationTest,
                       Apply_SwitchTab_ToWatermark_NoWait) {
  NavigateToAndWait(GURL("https://watermark.com"));

  // Create a second tab with a page that should not be watermarked. We
  // intentionally do not wait for the load to finish. The watermark should
  // not be showing.
  NavigateParams params(browser(), GURL("chrome://version"),
                        ui::PAGE_TRANSITION_LINK);
  params.tabstrip_index = 1;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());

  // Switch back to the watermarked tab. The watermark should still be showing.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  // Wait for the second (now backgrounded) tab to finish loading. The watermark
  // should still be showing.
  content::WaitForLoadStop(params.navigated_or_inserted_contents);
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());
}

IN_PROC_BROWSER_TEST_F(WatermarkBrowserNavigationTest,
                       Apply_SwitchTab_ToWatermark_PartialWait) {
  // Initial page should be watermarked.
  NavigateToAndWait(GURL("https://watermark.com"));
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  // Create a second tab. Navigate to a page that does not have a watermark.
  // Part way through the navigation, switch to the first tab again.
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  NavigateParams params(browser(), GURL("https://nowatermark.com"),
                        ui::PAGE_TRANSITION_LINK);
  params.tabstrip_index = 1;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());
  // Initial page loaded into the browser view is a chrome:// URL that has no
  // watermark.
  EXPECT_FALSE(browser_view->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());

  base::test::TestFuture<void> future;
  browser()
      ->GetActiveTabInterface()
      ->GetTabFeatures()
      ->data_protection_controller()
      ->SetCallbackForTesting(future.GetCallback());

  // Wait for the navigation to partially complete. The load is not complete but
  // DataProtectionViewController::ApplyDataProtectionSettings has been
  // called with the verdict to clear the watermark.
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(browser_view->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());

  // Switch back to the watermarked tab. The watermark should show immediately.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kMouse));
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  // Wait for the second (now backgrounded) tab to finish loading. The watermark
  // should still be showing.
  content::WaitForLoadStop(params.navigated_or_inserted_contents);
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());
}

IN_PROC_BROWSER_TEST_F(WatermarkBrowserNavigationTest, SplitTabWatermark) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  browser()->tab_strip_model()->ActivateTabAt(0);
  split_tabs::SplitTabId split_id = browser()->tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  NavigateToAndWait(GURL("https://watermark.com"));

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[1]
                   ->watermark_view()
                   ->has_text_for_testing());

  // Reverse the tabs in the split.
  browser()->tab_strip_model()->ReverseTabsInSplit(split_id);

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[1]
                  ->watermark_view()
                  ->has_text_for_testing());

  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());

  // Switch to a different tab without split.
  browser()->tab_strip_model()->ActivateTabAt(2);
  NavigateToAndWait(GURL("https://watermark.com"));

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());

  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[1]
                   ->GetVisible());

  // Switch back to split view .
  browser()->tab_strip_model()->ActivateTabAt(1);

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[1]
                  ->watermark_view()
                  ->has_text_for_testing());

  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(browser())
                   ->GetContentsContainerViews()[0]
                   ->watermark_view()
                   ->has_text_for_testing());

  // Add watermark to the other split view as well.
  browser()->tab_strip_model()->ActivateTabAt(0);
  NavigateToAndWait(GURL("https://watermark.com"));

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[1]
                  ->watermark_view()
                  ->has_text_for_testing());

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetContentsContainerViews()[0]
                  ->watermark_view()
                  ->has_text_for_testing());
}

// Test fixture for the dynamic chrome://watermark page tests. This is
// parameterized to test various style combinations.
class WatermarkTestPageDynamicBrowserTest
    : public UiBrowserTest,
      public testing::WithParamInterface<WatermarkStyleParams> {
 public:
  WatermarkTestPageDynamicBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kEnableWatermarkTestPage);
  }

  void ShowUi(const std::string& name) override {
    base::RunLoop().RunUntilIdle();
  }

  bool VerifyUi() override {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();

    const std::string name =
        std::string(test_info->name()) + "_" + GetParam().test_suffix;

    return VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                             ->contents_container(),
                         test_info->test_suite_name(),
                         name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WatermarkTestPageBrowserTest, InvokeUi_default) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIWatermarkURL)));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WatermarkTestPageDynamicBrowserTest, DynamicStyle) {
  const auto& params = GetParam();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIWatermarkURL)));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* watermark_ui =
      web_contents->GetWebUI()->GetController()->GetAs<WatermarkUI>();
  ASSERT_TRUE(watermark_ui);
  auto* page_handler = watermark_ui->GetPageHandlerForTesting();
  ASSERT_TRUE(page_handler);

  auto style = watermark::mojom::WatermarkStyle::New();
  style->fill_opacity = params.fill_opacity;
  style->outline_opacity = params.outline_opacity;
  style->font_size = params.font_size;
  page_handler->SetWatermarkStyle(std::move(style));

  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WatermarkTestPageDynamicBrowserTest,
    testing::Values(
        WatermarkStyleParams{"HighOpacity", /*fill_opacity=*/80,
                             /*outline_opacity=*/90, /*font_size=*/24},
        WatermarkStyleParams{"LargeFont", /*fill_opacity=*/4,
                             /*outline_opacity=*/6, /*font_size=*/72},
        WatermarkStyleParams{"ZeroOpacity", /*fill_opacity=*/0,
                             /*outline_opacity=*/0, /*font_size=*/24}));

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

IN_PROC_BROWSER_TEST_P(WatermarkSettingsBrowserTest, GetStyleSettings) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  // Test with default pref values.
  SkColor expected_fill_color = GetDefaultFillColor();
  SkColor expected_outline_color = GetDefaultOutlineColor();
  int expected_font_size = GetDefaultFontSize();

  EXPECT_EQ(GetFillColor(prefs), expected_fill_color);
  EXPECT_EQ(GetOutlineColor(prefs), expected_outline_color);
  EXPECT_EQ(GetFontSize(prefs), expected_font_size);

  // Test with custom pref values.
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleFillOpacityPref, 30);
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleOutlineOpacityPref,
                    40);
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref, 50);

  if (IsCustomizationEnabled()) {
    expected_fill_color =
        SkColorSetA(SkColorSetRGB(0x00, 0x00, 0x00), PercentageToSkAlpha(30));
    expected_outline_color =
        SkColorSetA(SkColorSetRGB(0xff, 0xff, 0xff), PercentageToSkAlpha(40));
    expected_font_size = 50;
  }

  EXPECT_EQ(GetFillColor(prefs), expected_fill_color);
  EXPECT_EQ(GetOutlineColor(prefs), expected_outline_color);
  EXPECT_EQ(GetFontSize(prefs), expected_font_size);
}

INSTANTIATE_TEST_SUITE_P(All, WatermarkSettingsBrowserTest, testing::Bool());
class WatermarkSettingsCommandLineBrowserTest : public InProcessBrowserTest {
 public:
  SkAlpha PercentageToSkAlpha(int percent_value) {
    return std::clamp(percent_value, 0, 100) * 255 / 100;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("watermark-fill-opacity", "50");
    command_line->AppendSwitchASCII("watermark-outline-opacity", "60");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kEnableWatermarkCustomization};
};

IN_PROC_BROWSER_TEST_F(WatermarkSettingsCommandLineBrowserTest, GetColors) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ(GetFillColor(prefs), SkColorSetA(SkColorSetRGB(0x00, 0x00, 0x00),
                                             PercentageToSkAlpha(50)));
  EXPECT_EQ(GetOutlineColor(prefs), SkColorSetA(SkColorSetRGB(0xff, 0xff, 0xff),
                                                PercentageToSkAlpha(60)));
}

}  // namespace enterprise_watermark
