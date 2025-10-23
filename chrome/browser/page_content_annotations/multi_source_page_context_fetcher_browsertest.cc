// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"

#include <optional>

#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace page_content_annotations {

using testing::_;
using testing::AllOf;
using testing::Conditional;
using testing::DistanceFrom;
using testing::Le;
using testing::ResultOf;

constexpr std::string_view kHostA = "a.test";
constexpr std::string_view kHostASubdomain = "foo.a.test";
constexpr std::string_view kHostB = "b.test";

int64_t Red(SkColor color) {
  return SkColorGetR(color);
}
int64_t Green(SkColor color) {
  return SkColorGetG(color);
}
int64_t Blue(SkColor color) {
  return SkColorGetB(color);
}

// Matches a Skia color, within a given tolerance.
MATCHER_P2(IsColorWithinTolerance, expected_color, tolerance, "") {
  return testing::ExplainMatchResult(
      AllOf(ResultOf("red component", &Red,
                     DistanceFrom(Red(expected_color), Le(tolerance))),
            ResultOf("green component", &Green,
                     DistanceFrom(Green(expected_color), Le(tolerance))),
            ResultOf("blue component", &Blue,
                     DistanceFrom(Blue(expected_color), Le(tolerance)))),
      arg, result_listener);
}

class MultiSourcePageContextFetcherBrowserTest : public InProcessBrowserTest {
 public:
  MultiSourcePageContextFetcherBrowserTest() = default;

  ~MultiSourcePageContextFetcherBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_https_test_server().ServeFilesFromDirectory(test_data_dir);
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL GetURL(std::string_view host, std::string_view path = "/") const {
    return embedded_https_test_server().GetURL(host, path);
  }

  content::RenderFrameHost* GetPrimaryMainFrame() const {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetSubframe() const {
    auto* frame = ChildFrameAt(GetPrimaryMainFrame(), 0);
    CHECK(frame);
    return frame;
  }

  void SetBackground(content::RenderFrameHost* frame, std::string_view color) {
    ASSERT_TRUE(content::ExecJs(
        frame, base::StrCat({
                   R"(document.body.setAttribute("style", "background-color:)",
                   color,
                   R"(");)",
               })));
  }
};

class ScreenshotBackendMultiSourcePageContextFetcherBrowserTest
    : public MultiSourcePageContextFetcherBrowserTest,
      public testing::WithParamInterface<std::optional<bool>> {
 public:
  ScreenshotBackendMultiSourcePageContextFetcherBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {kGlicTabScreenshotExperiment,
         {
             {"screenshot_timeout_ms", "10s"},
         }},
    };
    std::vector<base::test::FeatureRef> disabled_features;

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

  bool use_paint_preview_backend() const { return GetParam().has_value(); }

  // Only use if `use_paint_preview_backend()` is true.
  bool capture_full_page_screenshot() const { return GetParam().value(); }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ScreenshotBackendMultiSourcePageContextFetcherBrowserTest,
    ::testing::Values(
        // Use Paint Preview and take a full-page screenshot.
        std::optional(true),
        // Use Paint Preview and take a visible-rect screenshot.
        std::optional(false),
        // Don't use Paint Preview (use CopyFromSurface instead).
        std::nullopt));

IN_PROC_BROWSER_TEST_P(
    ScreenshotBackendMultiSourcePageContextFetcherBrowserTest,
    TakesScreenshot) {
  GURL url = embedded_https_test_server().GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  SetBackground(web_contents()->GetPrimaryMainFrame(), "red");

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;

  FetchPageContextOptions options;
  if (use_paint_preview_backend()) {
    options.screenshot_options =
        capture_full_page_screenshot()
            ? ScreenshotOptions::FullPage(PaintPreviewOptions())
            : ScreenshotOptions::ViewportOnly(PaintPreviewOptions());
  } else {
    options.screenshot_options =
        ScreenshotOptions::ViewportOnly(/*paint_preview_options=*/std::nullopt);
  }
  FetchPageContext(*web_contents(), options, nullptr, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FetchPageContextResult> result,
                       future.Take());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->screenshot_result.has_value());

  ScreenshotResult& screenshot = result->screenshot_result.value();

  // TODO(crbug.com/443783984): Add test coverage for the dimensions of the
  // screenshot.
  EXPECT_FALSE(screenshot.dimensions.IsZero());
  ASSERT_GT(screenshot.screenshot_data.size(), 0);
  ASSERT_EQ(screenshot.mime_type, "image/jpeg");

  SkBitmap bitmap = gfx::JPEGCodec::Decode(screenshot.screenshot_data);

  EXPECT_FALSE(bitmap.isNull());
  EXPECT_FALSE(bitmap.empty());

  // Sampling a pixel from the screenshot should give us red, within some error
  // bounds (due to lossy jpeg encoding/decoding).
  EXPECT_THAT(bitmap.getColor(10, 10),
              Conditional(use_paint_preview_backend(),
                          IsColorWithinTolerance(SK_ColorRED, 0x20),
                          // TODO(b/438825957): add test coverage for the output
                          // of the CopyFromSurface screenshot.
                          _));
}

class RedactingMultiSourcePageContextFetcherBrowserTest
    : public MultiSourcePageContextFetcherBrowserTest {
 public:
  RedactingMultiSourcePageContextFetcherBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {kGlicTabScreenshotExperiment,
         {
             {"screenshot_timeout_ms", "10s"},
         }},
    };
    features_.InitWithFeaturesAndParameters(enabled_features,
                                            /*disabled_features=*/{});
  }

  ~RedactingMultiSourcePageContextFetcherBrowserTest() override = default;

  ScreenshotOptions GetScreenshotOptionsWithCrossSiteIframeRedaction() const {
    PaintPreviewOptions paint_preview_options;
    paint_preview_options.iframe_redaction_scope =
        page_content_annotations::ScreenshotIframeRedactionScope::kCrossSite;
    ScreenshotOptions options =
        ScreenshotOptions::ViewportOnly(paint_preview_options);
    return options;
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(RedactingMultiSourcePageContextFetcherBrowserTest,
                       TakesScreenshot_SameOriginIframeNoRedaction) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL(kHostA, "/iframe.html")));

  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents(), "test", GetURL(kHostA)));
  SetBackground(web_contents()->GetPrimaryMainFrame(), "white");
  // Remove the 8px margin applied by the user agent's stylesheet.
  EXPECT_TRUE(
      content::ExecJs(web_contents(), "document.body.style.margin = '0px';"));
  SetBackground(GetSubframe(), "red");

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.screenshot_options =
      GetScreenshotOptionsWithCrossSiteIframeRedaction();

  FetchPageContext(*web_contents(), options, nullptr, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FetchPageContextResult> result,
                       future.Take());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->screenshot_result.has_value());

  ScreenshotResult& screenshot = result->screenshot_result.value();

  EXPECT_FALSE(screenshot.dimensions.IsZero());
  ASSERT_GT(screenshot.screenshot_data.size(), 0);
  ASSERT_EQ(screenshot.mime_type, "image/jpeg");

  SkBitmap bitmap = gfx::JPEGCodec::Decode(screenshot.screenshot_data);

  EXPECT_FALSE(bitmap.isNull());
  EXPECT_FALSE(bitmap.empty());
  EXPECT_THAT(bitmap.getColor(10, 10),
              IsColorWithinTolerance(SK_ColorRED, 0x20));
}

IN_PROC_BROWSER_TEST_F(RedactingMultiSourcePageContextFetcherBrowserTest,
                       TakesScreenshot_CrossOriginSameSiteIframeNoRedaction) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL(kHostA, "/iframe.html")));

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(), "test",
                                           GetURL(kHostASubdomain)));
  SetBackground(web_contents()->GetPrimaryMainFrame(), "white");
  // Remove the 8px margin applied by the user agent's stylesheet.
  EXPECT_TRUE(
      content::ExecJs(web_contents(), "document.body.style.margin = '0px';"));
  SetBackground(GetSubframe(), "red");

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.screenshot_options =
      GetScreenshotOptionsWithCrossSiteIframeRedaction();

  FetchPageContext(*web_contents(), options, nullptr, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FetchPageContextResult> result,
                       future.Take());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->screenshot_result.has_value());

  ScreenshotResult& screenshot = result->screenshot_result.value();

  EXPECT_FALSE(screenshot.dimensions.IsZero());
  ASSERT_GT(screenshot.screenshot_data.size(), 0);
  ASSERT_EQ(screenshot.mime_type, "image/jpeg");

  SkBitmap bitmap = gfx::JPEGCodec::Decode(screenshot.screenshot_data);

  EXPECT_FALSE(bitmap.isNull());
  EXPECT_FALSE(bitmap.empty());
  EXPECT_THAT(bitmap.getColor(10, 10),
              IsColorWithinTolerance(SK_ColorRED, 0x20));
}

IN_PROC_BROWSER_TEST_F(RedactingMultiSourcePageContextFetcherBrowserTest,
                       TakesScreenshot_CrossSiteIframeRedacted) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL(kHostA, "/iframe.html")));

  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents(), "test", GetURL(kHostB)));
  SetBackground(web_contents()->GetPrimaryMainFrame(), "white");
  // Remove the 8px margin applied by the user agent's stylesheet.
  EXPECT_TRUE(
      content::ExecJs(web_contents(), "document.body.style.margin = '0px';"));
  SetBackground(GetSubframe(), "red");

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;
  FetchPageContextOptions options;
  options.screenshot_options =
      GetScreenshotOptionsWithCrossSiteIframeRedaction();

  FetchPageContext(*web_contents(), options, nullptr, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FetchPageContextResult> result,
                       future.Take());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->screenshot_result.has_value());

  ScreenshotResult& screenshot = result->screenshot_result.value();

  EXPECT_FALSE(screenshot.dimensions.IsZero());
  ASSERT_GT(screenshot.screenshot_data.size(), 0);
  ASSERT_EQ(screenshot.mime_type, "image/jpeg");

  SkBitmap bitmap = gfx::JPEGCodec::Decode(screenshot.screenshot_data);

  EXPECT_FALSE(bitmap.isNull());
  EXPECT_FALSE(bitmap.empty());
  EXPECT_THAT(bitmap.getColor(10, 10),
              IsColorWithinTolerance(SK_ColorBLACK, 0x20));
}

// Test class that sets png params and validates pngs are returned.
class PngMultiSourcePageContextFetcherBrowserTest
    : public MultiSourcePageContextFetcherBrowserTest {
 public:
  PngMultiSourcePageContextFetcherBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {kGlicTabScreenshotExperiment,
         {
             {"screenshot_timeout_ms", "10s"},
             {"screenshot_image_type", "png"},
         }},
    };
    features_.InitWithFeaturesAndParameters(enabled_features,
                                            /*disabled_features=*/{});
  }
  ~PngMultiSourcePageContextFetcherBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that the mimetype returned is png and image decodes correctly.
IN_PROC_BROWSER_TEST_F(PngMultiSourcePageContextFetcherBrowserTest,
                       TakesScreenshot_Png) {
  GURL url = embedded_https_test_server().GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  SetBackground(web_contents()->GetPrimaryMainFrame(), "red");

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;

  FetchPageContextOptions options;

  options.screenshot_options =
      ScreenshotOptions::ViewportOnly(/*paint_preview_options=*/std::nullopt);
  FetchPageContext(*web_contents(), options, nullptr, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FetchPageContextResult> result,
                       future.Take());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->screenshot_result.has_value());

  ScreenshotResult& screenshot = result->screenshot_result.value();

  EXPECT_FALSE(screenshot.dimensions.IsZero());
  ASSERT_GT(screenshot.screenshot_data.size(), 0);
  ASSERT_EQ(screenshot.mime_type, "image/png");

  SkBitmap bitmap = gfx::PNGCodec::Decode(screenshot.screenshot_data);

  EXPECT_FALSE(bitmap.isNull());
  EXPECT_FALSE(bitmap.empty());
}

// Test class that sets webp params and validates webps are returned.
class WebpMultiSourcePageContextFetcherBrowserTest
    : public MultiSourcePageContextFetcherBrowserTest {
 public:
  WebpMultiSourcePageContextFetcherBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {kGlicTabScreenshotExperiment,
         {
             {"screenshot_timeout_ms", "10s"},
             {"screenshot_image_type", "webp"},
         }},
    };
    features_.InitWithFeaturesAndParameters(enabled_features,
                                            /*disabled_features=*/{});
  }
  ~WebpMultiSourcePageContextFetcherBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that the mimetype returned is webp and image decodes correctly.
IN_PROC_BROWSER_TEST_F(WebpMultiSourcePageContextFetcherBrowserTest,
                       TakesScreenshot_Webp) {
  GURL url = embedded_https_test_server().GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  SetBackground(web_contents()->GetPrimaryMainFrame(), "red");

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;

  FetchPageContextOptions options;

  options.screenshot_options =
      ScreenshotOptions::ViewportOnly(/*paint_preview_options=*/std::nullopt);
  FetchPageContext(*web_contents(), options, nullptr, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FetchPageContextResult> result,
                       future.Take());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->screenshot_result.has_value());

  ScreenshotResult& screenshot = result->screenshot_result.value();

  EXPECT_FALSE(screenshot.dimensions.IsZero());
  ASSERT_GT(screenshot.screenshot_data.size(), 0);
  ASSERT_EQ(screenshot.mime_type, "image/webp");
}

}  // namespace page_content_annotations
