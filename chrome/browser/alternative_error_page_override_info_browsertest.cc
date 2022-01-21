// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "skia/ext/skia_utils_base.h"
#include "url/gurl.h"

// Class to test browser error page display info.
class AlternativeErrorPageOverrideInfoBrowserTest
    : public InProcessBrowserTest {
 public:
  AlternativeErrorPageOverrideInfoBrowserTest() = default;

  // Helper function to prepare PWA and retrieve information from the
  // alternative error page function.
  content::mojom::AlternativeErrorPageOverrideInfoPtr GetErrorPageInfo(
      std::string html) {
    ChromeContentBrowserClient browser_client;
    content::ScopedContentBrowserClientSetting setting(&browser_client);

    const GURL app_url = embedded_test_server()->GetURL(html);
    web_app::NavigateToURLAndWait(browser(), app_url);
    web_app::test::InstallPwaForCurrentUrl(browser());
    content::BrowserContext* context = browser()->profile();

    return browser_client.GetAlternativeErrorPageOverrideInfo(app_url, context);
  }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_{
      features::kDesktopPWAsDefaultOfflinePage};
};

// Testing app manifest with no theme or background color.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest, Manifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/manifest_no_service_worker.html");

  // Expect mojom struct with default background and theme colors.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("background_color"),
            base::Value(skia::SkColorToHexString(SK_ColorWHITE)));
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("theme_color"),
            base::Value(skia::SkColorToHexString(SK_ColorBLACK)));
}

// Testing app manifest with theme color.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithThemeColor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/theme-color.html");

  // Expect mojom struct with customized theme color and default background
  // color.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("background_color"),
            base::Value(skia::SkColorToHexString(SK_ColorWHITE)));
  EXPECT_EQ(
      *info->alternative_error_page_params.FindKey("theme_color"),
      base::Value(skia::SkColorToHexString(SkColorSetRGB(0xAA, 0xCC, 0xEE))));
}

// Testing app manifest with background color.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithBackgroundColor) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/background-color.html");

  // Expect mojom struct with default theme color and customized background
  // color.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("background_color"),
            base::Value(skia::SkColorToHexString(SK_ColorBLUE)));
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("theme_color"),
            base::Value(skia::SkColorToHexString(SK_ColorBLACK)));
}

// Testing url outside the scope of an installed app.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       NoManifest) {
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/simple.html");
  content::BrowserContext* context = browser()->profile();

  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(app_url, context);

  // Expect mojom struct to be null.
  EXPECT_FALSE(info);
}

// Testing manifest with app short name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithAppShortName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_short_name_only.json");

  // Expect mojom struct with custom app short name.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value("Manifest"));
}

// Testing app manifest with no app short name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest.json");

  // Expect mojom struct with customized with app name.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value("Manifest test app"));
}

// Testing app manifest with no app short name or app name.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortNameOrAppName) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info = GetErrorPageInfo(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_empty_name_short_name.json");

  // Expect mojom struct customized with HTML page title.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value("Web app banner test page"));
}

// Testing app manifest with no app short name or app name, and HTML page
// has no title
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestWithNoAppShortNameOrAppNameOrTitle) {
  ChromeContentBrowserClient browser_client;
  content::ScopedContentBrowserClientSetting setting(&browser_client);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/title1.html");
  web_app::NavigateToURLAndWait(browser(), app_url);
  web_app::test::InstallPwaForCurrentUrl(browser());
  content::BrowserContext* context = browser()->profile();

  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      browser_client.GetAlternativeErrorPageOverrideInfo(app_url, context);

  // Expect mojom struct customized with HTML page title.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("app_short_name"),
            base::Value(url_formatter::FormatUrl(app_url)));
}

// Testing app with manifest and no service worker.
IN_PROC_BROWSER_TEST_F(AlternativeErrorPageOverrideInfoBrowserTest,
                       ManifestAndNoServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::mojom::AlternativeErrorPageOverrideInfoPtr info =
      GetErrorPageInfo("/banners/no-sw-with-colors.html");

  // Expect mojom struct with custom theme and background color.
  EXPECT_TRUE(info);
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("background_color"),
            base::Value(skia::SkColorToHexString(SK_ColorYELLOW)));
  EXPECT_EQ(*info->alternative_error_page_params.FindKey("theme_color"),
            base::Value(skia::SkColorToHexString(SK_ColorGREEN)));
}
