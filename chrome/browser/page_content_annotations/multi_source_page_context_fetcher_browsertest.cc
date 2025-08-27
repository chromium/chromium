// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"

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

namespace page_content_annotations {

using testing::_;
using testing::AllOf;
using testing::Conditional;
using testing::DistanceFrom;
using testing::Le;
using testing::ResultOf;

uint32_t Red(SkColor color) {
  return SkColorGetR(color);
}
uint32_t Green(SkColor color) {
  return SkColorGetG(color);
}
uint32_t Blue(SkColor color) {
  return SkColorGetB(color);
}

// Matches a Skia color, within a given tolerance.
MATCHER_P2(IsColorWithinTolerance, color, tolerance, "") {
  int64_t expected_red = SkColorGetR(color);
  int64_t expected_green = SkColorGetG(color);
  int64_t expected_blue = SkColorGetB(color);

  return testing::ExplainMatchResult(
      AllOf(ResultOf("red component", &Red,
                     DistanceFrom(expected_red, Le(tolerance))),
            ResultOf("green component", &Green,
                     DistanceFrom(expected_green, Le(tolerance))),
            ResultOf("blue component", &Blue,
                     DistanceFrom(expected_blue, Le(tolerance)))),
      arg, result_listener);
}

class MultiSourcePageContextFetcherBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  MultiSourcePageContextFetcherBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {kGlicTabScreenshotExperiment,
         {
             {"max_screenshot_width", "0"},
             {"max_screenshot_height", "0"},
             {"screenshot_jpeg_quality", "100"},
             {"screenshot_timeout_ms", "10s"},
         }},
    };
    std::vector<base::test::FeatureRef> disabled_features;
    if (use_paint_preview_screenshot_backend()) {
      enabled_features.push_back({kGlicTabScreenshotPaintPreviewBackend, {}});
    } else {
      disabled_features.emplace_back(kGlicTabScreenshotPaintPreviewBackend);
    }

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

  ~MultiSourcePageContextFetcherBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_https_test_server().ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  bool use_paint_preview_screenshot_backend() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(,
                         MultiSourcePageContextFetcherBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(MultiSourcePageContextFetcherBrowserTest,
                       TakesScreenshot) {
  GURL url = embedded_https_test_server().GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::ExecJs(
      web_contents,
      R"(document.body.setAttribute("style", "background-color:red");)"));

  base::test::TestFuture<FetchPageContextResultCallbackArg> future;

  FetchPageContextOptions options;
  options.include_viewport_screenshot = true;
  FetchPageContext(*web_contents, options, future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FetchPageContextResult> result,
                       future.Take());

  ASSERT_TRUE(result);
  std::optional<ScreenshotResult>& screenshot = result->screenshot_result;
  ASSERT_TRUE(screenshot);

  EXPECT_FALSE(screenshot->dimensions.IsZero());
  ASSERT_GT(screenshot->jpeg_data.size(), 0);

  SkBitmap bitmap = gfx::JPEGCodec::Decode(screenshot->jpeg_data);

  EXPECT_FALSE(bitmap.isNull());
  EXPECT_FALSE(bitmap.empty());

  // Sampling a pixel from the screenshot should give us red, within some error
  // bounds (due to lossy jpeg encoding/decoding).
  EXPECT_THAT(bitmap.getColor(10, 10),
              Conditional(use_paint_preview_screenshot_backend(),
                          IsColorWithinTolerance(SK_ColorRED, 0x10),
                          // TODO(b/438825957): add test coverage for the output
                          // of the CopyFromSurface screenshot.
                          _));
}

}  // namespace page_content_annotations
