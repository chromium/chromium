// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/headless/command_handler/headless_command_handler.h"
#include "components/headless/command_handler/headless_command_switches.h"
#include "components/headless/test/bitmap_utils.h"
#include "components/headless/test/capture_std_stream.h"
#include "components/headless/test/pdf_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace headless {

namespace {
bool DecodePNG(const std::string& png_data, SkBitmap* bitmap) {
  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(png_data.data()), png_data.size(),
      bitmap);
}
}  // namespace

class HeadlessModeCommandBrowserTest : public HeadlessModeBrowserTest {
 public:
  HeadlessModeCommandBrowserTest() = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    HeadlessCommandHandler::SetDoneCallbackForTesting(base::BindOnce(
        &HeadlessModeCommandBrowserTest::FinishTest, base::Unretained(this)));

    HeadlessModeBrowserTest::SetUp();
  }

 protected:
  GURL GetTargetUrl(const std::string& url) {
    return embedded_test_server()->GetURL(url);
  }

  void RunLoop() {
    if (!test_complete_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  bool test_complete() const { return test_complete_; }

 private:
  void FinishTest() {
    test_complete_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool test_complete_ = false;
};

class HeadlessModeCommandBrowserTestWithTempDir
    : public HeadlessModeCommandBrowserTest {
 public:
  HeadlessModeCommandBrowserTestWithTempDir() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::IsDirectoryEmpty(temp_dir()));

    HeadlessModeCommandBrowserTest::SetUp();
  }

  void TearDown() override {
    HeadlessModeCommandBrowserTest::TearDown();

    ASSERT_TRUE(temp_dir_.Delete());
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// DumpDom command tests ----------------------------------------------

class HeadlessModeDumpDomCommandBrowserTest
    : public HeadlessModeCommandBrowserTest {
 public:
  HeadlessModeDumpDomCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeCommandBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDumpDom);
    command_line->AppendArg(GetTargetUrl("/hello.html").spec());

    capture_stdout_.StartCapture();
  }

 protected:
  CaptureStdOut capture_stdout_;
};

// TODO(crbug.com/1440917): Reenable once deflaked.
#if BUILDFLAG(IS_MAC)
#define MAYBE_HeadlessDumpDom DISABLED_HeadlessDumpDom
#else
#define MAYBE_HeadlessDumpDom HeadlessDumpDom
#endif
IN_PROC_BROWSER_TEST_F(HeadlessModeDumpDomCommandBrowserTest,
                       MAYBE_HeadlessDumpDom) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunLoop();
  capture_stdout_.StopCapture();

  std::string captured_stdout = capture_stdout_.TakeCapturedData();

  static const char kDomDump[] =
      "<!DOCTYPE html>\n"
      "<html><head></head><body><h1>Hello headless world!</h1>\n"
      "</body></html>\n";
  EXPECT_THAT(captured_stdout, testing::HasSubstr(kDomDump));
}

class HeadlessModeDumpDomCommandBrowserTestWithTimeout
    : public HeadlessModeDumpDomCommandBrowserTest {
 public:
  HeadlessModeDumpDomCommandBrowserTestWithTimeout() = default;

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HeadlessModeDumpDomCommandBrowserTestWithTimeout::RequestHandler,
        base::Unretained(this)));

    HeadlessModeDumpDomCommandBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeDumpDomCommandBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kTimeout, "1000");
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/hello.html") {
      // The target page is opened first from the browser startup sequence and
      // then again from the command handler. We want to delay only the second
      // request until the command processing is done.
      if (++hello_request_number_ == 2) {
        while (!test_complete()) {
          base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
        }
      }

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("<body>Hello headless world!</body>");
    }

    return nullptr;
  }

  int hello_request_number_ = 0;
};

// TODO(crbug.com/1446617): Reenable once deflaked.
IN_PROC_BROWSER_TEST_F(HeadlessModeDumpDomCommandBrowserTestWithTimeout,
                       DISABLED_HeadlessDumpDomWithTimeout) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunLoop();

  capture_stdout_.StopCapture();

  std::string captured_stdout = capture_stdout_.TakeCapturedData();

  // Expect about:blank DOM, not the one we might have returned after a delay.
  EXPECT_THAT(captured_stdout,
              testing::HasSubstr("<html><head></head><body></body></html>"));
}

// Screenshot command tests -------------------------------------------

class HeadlessModeScreenshotCommandBrowserTest
    : public HeadlessModeCommandBrowserTestWithTempDir {
 public:
  HeadlessModeScreenshotCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeCommandBrowserTestWithTempDir::SetUpCommandLine(command_line);

    screenshot_filename_ =
        temp_dir().Append(FILE_PATH_LITERAL("screenshot.png"));
    command_line->AppendSwitchPath(switches::kScreenshot, screenshot_filename_);
    command_line->AppendArg(GetTargetUrl("/centered_blue_box.html").spec());
  }

 protected:
  base::FilePath screenshot_filename_;
};

// TODO(crbug.com/1442606): Disabled due to flakiness on Windows ASAN.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HeadlessScreenshot DISABLED_HeadlessScreenshot
#else
#define MAYBE_HeadlessScreenshot HeadlessScreenshot
#endif
IN_PROC_BROWSER_TEST_F(HeadlessModeScreenshotCommandBrowserTest,
                       MAYBE_HeadlessScreenshot) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunLoop();

  ASSERT_TRUE(base::PathExists(screenshot_filename_)) << screenshot_filename_;

  std::string png_data;
  ASSERT_TRUE(base::ReadFileToString(screenshot_filename_, &png_data))
      << screenshot_filename_;

  SkBitmap bitmap;
  ASSERT_TRUE(DecodePNG(png_data, &bitmap));

  // Expect a centered blue rectangle on white background.
  EXPECT_TRUE(CheckColoredRect(bitmap, SkColorSetRGB(0x00, 0x00, 0xff),
                               SkColorSetRGB(0xff, 0xff, 0xff)));
}

// PrintToPDF command tests -------------------------------------------

class HeadlessModePrintToPdfCommandBrowserTestBase
    : public HeadlessModeCommandBrowserTestWithTempDir {
 public:
  HeadlessModePrintToPdfCommandBrowserTestBase() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeCommandBrowserTestWithTempDir::SetUpCommandLine(command_line);

    print_to_pdf_filename_ =
        temp_dir().Append(FILE_PATH_LITERAL("print_to.pdf"));
    command_line->AppendSwitchPath(switches::kPrintToPDF,
                                   print_to_pdf_filename_);
    command_line->AppendSwitch(switches::kNoPDFHeaderFooter);
  }

 protected:
  base::FilePath print_to_pdf_filename_;
};

class HeadlessModePrintToPdfCommandBrowserTest
    : public HeadlessModePrintToPdfCommandBrowserTestBase {
 public:
  HeadlessModePrintToPdfCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModePrintToPdfCommandBrowserTestBase::SetUpCommandLine(
        command_line);

    command_line->AppendArg(GetTargetUrl("/centered_blue_box.html").spec());
  }
};

// TODO(crbug.com/1440917): Reenable once deflaked.
#if BUILDFLAG(IS_MAC)
#define MAYBE_HeadlessPrintToPdf DISABLED_HeadlessPrintToPdf
#else
#define MAYBE_HeadlessPrintToPdf HeadlessPrintToPdf
#endif
IN_PROC_BROWSER_TEST_F(HeadlessModePrintToPdfCommandBrowserTest,
                       MAYBE_HeadlessPrintToPdf) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunLoop();

  ASSERT_TRUE(base::PathExists(print_to_pdf_filename_))
      << print_to_pdf_filename_;

  std::string pdf_data;
  ASSERT_TRUE(base::ReadFileToString(print_to_pdf_filename_, &pdf_data))
      << print_to_pdf_filename_;

  PDFPageBitmap page_bitmap;
  ASSERT_TRUE(page_bitmap.Render(pdf_data, /*page_index=*/0));

  // Expect blue rectangle on white background.
  EXPECT_TRUE(page_bitmap.CheckColoredRect(SkColorSetRGB(0x00, 0x00, 0xff),
                                           SkColorSetRGB(0xff, 0xff, 0xff)));
}

class HeadlessModeLazyLoadingPrintToPdfCommandBrowserTest
    : public HeadlessModePrintToPdfCommandBrowserTestBase {
 public:
  HeadlessModeLazyLoadingPrintToPdfCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModePrintToPdfCommandBrowserTestBase::SetUpCommandLine(
        command_line);
    command_line->AppendArg(GetTargetUrl("/page_with_lazy_image.html").spec());
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessModeLazyLoadingPrintToPdfCommandBrowserTest,
                       HeadlessLazyLoadingPrintToPdf) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunLoop();

  ASSERT_TRUE(base::PathExists(print_to_pdf_filename_))
      << print_to_pdf_filename_;

  std::string pdf_data;
  ASSERT_TRUE(base::ReadFileToString(print_to_pdf_filename_, &pdf_data))
      << print_to_pdf_filename_;

  PDFPageBitmap page_bitmap;
  ASSERT_TRUE(page_bitmap.Render(pdf_data, /*page_index=*/4));

  // Expect green rectangle on white background.
  EXPECT_TRUE(page_bitmap.CheckColoredRect(SkColorSetRGB(0x00, 0x64, 0x00),
                                           SkColorSetRGB(0xff, 0xff, 0xff)));
}

}  // namespace headless
