// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class ImageLoadingUMATest : public InProcessBrowserTest {
 public:
  ImageLoadingUMATest() = default;
  ImageLoadingUMATest(const ImageLoadingUMATest&) = delete;
  ImageLoadingUMATest& operator=(const ImageLoadingUMATest&) = delete;
  ~ImageLoadingUMATest() override = default;

 protected:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void run_test(std::string html) {
    const char kHtmlHttpResponseHeader[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n";
    const char kStylesheetHttpResponseHeader[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/css\r\n"
        "\r\n";
    const char kImgHttpResponseHeader[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/jpg\r\n"
        "\r\n";
    auto html_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/mock_page.html",
            true /*relative_url_is_prefix*/);
    auto style_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/styles/mock.css",
            false /*relative_url_is_prefix*/);
    auto img_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/images/mock.jpg",
            false /*relative_url_is_prefix*/);

    ASSERT_TRUE(embedded_test_server()->Start());

    // File is under content/test/data/
    // It is 120x120 pixels
    std::string file_contents;
    {
      base::ScopedAllowBlockingForTesting allow_io;
      base::FilePath test_dir;
      ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_dir));
      base::FilePath file_name = test_dir.AppendASCII("single_face.jpg");
      ASSERT_TRUE(base::ReadFileToString(file_name, &file_contents));
    }

    content::WebContents* contents = browser()->OpenURL(
        content::OpenURLParams(
            embedded_test_server()->GetURL("/mock_page.html"),
            content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            contents, "waiter");
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);

    html_response->WaitForRequest();
    html_response->Send(kHtmlHttpResponseHeader);
    html_response->Send(html.c_str());
    html_response->Done();

    img_response->WaitForRequest();
    img_response->Send(kImgHttpResponseHeader);
    img_response->Send(file_contents);
    img_response->Done();

    style_response->WaitForRequest();
    // Make sure histograms were not yet reported, because rendering didn't yet
    // happen.
    auto entries =
        histogram_tester_->GetTotalCountsForPrefix("Renderer.Images.");
    EXPECT_EQ(entries.size(), 0u);

    style_response->Send(kStylesheetHttpResponseHeader);
    style_response->Send("");
    style_response->Done();

    // Wait for LCP on the browser side.
    waiter->Wait();
    // Navigate away to ensure page load metrics reporting.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    // Actually fetch histogram data.
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    // Verify that we got the UMA histograms.
    entries = histogram_tester_->GetTotalCountsForPrefix("Renderer.Images.");
    EXPECT_NE(entries.size(), 0u);
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ImageLoadingUMATest, NoAttributeImage) {
  run_test(R"HTML(
    <!doctype html>
    <html>
      <link href='/styles/mock.css' rel=stylesheet>
      <img src='/images/mock.jpg'>
    </html>
    )HTML");
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedPixels", 0);
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedCappedPixels",
                                      0);
  histogram_tester_->ExpectTotalCount("Renderer.Images.SizesAttributeMiss", 0);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasOverfetchedPixels",
                                        false, 1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.HasOverfetchedCappedPixels", false, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.HasSizesAttributeMiss",
                                      0);
}

IN_PROC_BROWSER_TEST_F(ImageLoadingUMATest, StyledImage) {
  run_test(R"HTML(
    <!doctype html>
    <html>
      <link href='/styles/mock.css' rel=stylesheet>
      <style>
        img {max-width: 100%;}
        .container {width: 50px;}
      </style>
      <div class=container>
        <img src='/images/mock.jpg'>
      </div>
    </html>
    )HTML");
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedPixels", 1);
  // 11900 == 120*120 - 50*50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.OverfetchedPixels",
                                        11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedCappedPixels",
                                      1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.OverfetchedCappedPixels", 11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.SizesAttributeMiss", 0);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasOverfetchedPixels",
                                        true, 1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.HasOverfetchedCappedPixels", true, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.HasSizesAttributeMiss",
                                      0);
}

IN_PROC_BROWSER_TEST_F(ImageLoadingUMATest, ImageWithWidthAttribute) {
  run_test(R"HTML(
    <!doctype html>
    <html>
      <link href='/styles/mock.css' rel=stylesheet>
      <img src='/images/mock.jpg' width=50>
    </html>
    )HTML");
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedPixels", 1);
  // 11900 == 120*120 - 50*50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.OverfetchedPixels",
                                        11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedCappedPixels",
                                      1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.OverfetchedCappedPixels", 11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.SizesAttributeMiss", 0);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasOverfetchedPixels",
                                        true, 1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.HasOverfetchedCappedPixels", true, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.HasSizesAttributeMiss",
                                      0);
}

IN_PROC_BROWSER_TEST_F(ImageLoadingUMATest, ImageWithCorrectSizesAttribute) {
  run_test(R"HTML(
    <!doctype html>
    <html>
      <link href='/styles/mock.css' rel=stylesheet>
      <img srcset='/images/mock.jpg 120w' sizes=50px>
    </html>
    )HTML");
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedPixels", 1);
  // 11900 == 120*120 - 50*50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.OverfetchedPixels",
                                        11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedCappedPixels",
                                      1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.OverfetchedCappedPixels", 11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.SizesAttributeMiss", 0);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasOverfetchedPixels",
                                        true, 1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.HasOverfetchedCappedPixels", true, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.HasSizesAttributeMiss",
                                      1);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasSizesAttributeMiss",
                                        false, 1);
}

IN_PROC_BROWSER_TEST_F(ImageLoadingUMATest, ImageWithIncorrectSizesAttribute) {
  run_test(R"HTML(
    <!doctype html>
    <html>
      <link href='/styles/mock.css' rel=stylesheet>
      <style>
        img {max-width: 100%;}
        .container {width: 50px;}
      </style>
      <div class=container>
        <img srcset='/images/mock.jpg 120w' sizes=120px>
      </div>
    </html>
    )HTML");
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedPixels", 1);
  // 11900 == 120*120 - 50*50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.OverfetchedPixels",
                                        11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedCappedPixels",
                                      1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.OverfetchedCappedPixels", 11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.SizesAttributeMiss", 1);
  // 70 = 120 - 50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.SizesAttributeMiss",
                                        70, 1);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasOverfetchedPixels",
                                        true, 1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.HasOverfetchedCappedPixels", true, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.HasSizesAttributeMiss",
                                      1);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasSizesAttributeMiss",
                                        true, 1);
}

// TODO(crbug.com/40916617): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PictureWithIncorrectSizesAttribute \
  DISABLED_PictureWithIncorrectSizesAttribute
#else
#define MAYBE_PictureWithIncorrectSizesAttribute \
  PictureWithIncorrectSizesAttribute
#endif
IN_PROC_BROWSER_TEST_F(ImageLoadingUMATest,
                       MAYBE_PictureWithIncorrectSizesAttribute) {
  run_test(R"HTML(
    <!doctype html>
    <html>
      <link href='/styles/mock.css' rel=stylesheet>
      <style>
        img {max-width: 100%;}
        .container {width: 50px;}
      </style>
      <div class=container>
        <picture>
          <source srcset='/images/mock.jpg 120w' sizes=120px>
          <img>
        </picture>
      </div>
    </html>
    )HTML");
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedPixels", 1);
  // 11900 == 120*120 - 50*50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.OverfetchedPixels",
                                        11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedCappedPixels",
                                      1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.OverfetchedCappedPixels", 11900, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.SizesAttributeMiss", 1);
  // 70 = 120 - 50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.SizesAttributeMiss",
                                        70, 1);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasOverfetchedPixels",
                                        true, 1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.HasOverfetchedCappedPixels", true, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.HasSizesAttributeMiss",
                                      1);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasSizesAttributeMiss",
                                        true, 1);
}

class ImageLoadingUMAHighDPITest : public ImageLoadingUMATest {
 public:
  const double kDeviceScaleFactor = 2.1;

  ImageLoadingUMAHighDPITest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ImageLoadingUMATest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

IN_PROC_BROWSER_TEST_F(ImageLoadingUMAHighDPITest,
                       PictureWithIncorrectSizesAttribute) {
  run_test(R"HTML(
    <!doctype html>
    <html>
      <link href='/styles/mock.css' rel=stylesheet>
      <style>
        img {max-width: 100%;}
        .container {width: 50px;}
      </style>
      <div class=container>
        <picture>
          <source srcset='/images/mock.jpg 120w' sizes=120px>
          <img>
        </picture>
      </div>
    </html>
    )HTML");
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedPixels", 1);
  // 3375 == 120*120 - 50*2.1*50*2.1
  histogram_tester_->ExpectUniqueSample("Renderer.Images.OverfetchedPixels",
                                        3375, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.OverfetchedCappedPixels",
                                      1);
  // 4400 == 120*120 - 50*2*50*2
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.OverfetchedCappedPixels", 4400, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.SizesAttributeMiss", 1);
  // 70 = 120 - 50
  histogram_tester_->ExpectUniqueSample("Renderer.Images.SizesAttributeMiss",
                                        70, 1);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasOverfetchedPixels",
                                        true, 1);
  histogram_tester_->ExpectUniqueSample(
      "Renderer.Images.HasOverfetchedCappedPixels", true, 1);
  histogram_tester_->ExpectTotalCount("Renderer.Images.HasSizesAttributeMiss",
                                      1);
  histogram_tester_->ExpectUniqueSample("Renderer.Images.HasSizesAttributeMiss",
                                        true, 1);
}
