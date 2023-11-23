// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/test/test_timeouts.h"
#include "base/test/values_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/headless/command_handler/headless_command_handler.h"
#include "components/headless/command_handler/headless_command_switches.h"
#include "components/headless/test/bitmap_utils.h"
#include "components/headless/test/capture_std_stream.h"
#include "components/headless/test/pdf_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/pdf.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  absl::optional<HeadlessCommandHandler::Result> ProcessCommands() {
    if (!test_complete_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
    }

    return result_;
  }

 private:
  void FinishTest(HeadlessCommandHandler::Result result) {
    result_ = result;
    test_complete_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool test_complete_ = false;
  absl::optional<HeadlessCommandHandler::Result> result_;
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
    command_line->AppendArg(GetTargetUrl(GetTargetPage()).spec());

    capture_stdout_.StartCapture();
  }

  virtual std::string GetTargetPage() { return "/hello.html"; }

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
  ASSERT_THAT(ProcessCommands(),
              testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  capture_stdout_.StopCapture();
  std::string captured_stdout = capture_stdout_.TakeCapturedData();

  static const char kDomDump[] =
      "<!DOCTYPE html>\n"
      "<html><head></head><body><h1>Hello headless world!</h1>\n"
      "</body></html>\n";
  EXPECT_THAT(captured_stdout, testing::HasSubstr(kDomDump));
}

class HeadlessModeDumpDomCommandBrowserTestWithTimeoutBase
    : public HeadlessModeDumpDomCommandBrowserTest {
 public:
  HeadlessModeDumpDomCommandBrowserTestWithTimeoutBase() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeDumpDomCommandBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kTimeout,
                                    base::ToString(timeout().InMilliseconds()));
  }

  base::TimeDelta timeout() { return TestTimeouts::action_timeout(); }
};

class HeadlessModeDumpDomCommandBrowserTestWithTimeout
    : public HeadlessModeDumpDomCommandBrowserTestWithTimeoutBase {
 public:
  HeadlessModeDumpDomCommandBrowserTestWithTimeout() = default;

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HeadlessModeDumpDomCommandBrowserTestWithTimeout::RequestHandler,
        base::Unretained(this)));

    HeadlessModeDumpDomCommandBrowserTestWithTimeoutBase::SetUp();
  }

 private:
  std::string GetTargetPage() override { return "/page.html"; }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/page.html") {
      auto response = std::make_unique<net::test_server::DelayedHttpResponse>(
          timeout() * 2);
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(R"(<div>Hi, I'm headless!</div>)");

      return response;
    }

    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessModeDumpDomCommandBrowserTestWithTimeout,
                       HeadlessDumpDomWithTimeout) {
  // Main page timeout should be reported.
  EXPECT_THAT(ProcessCommands(),
              testing::Eq(HeadlessCommandHandler::Result::kPageLoadTimeout));

  capture_stdout_.StopCapture();
  std::string captured_stdout = capture_stdout_.TakeCapturedData();
  std::erase_if(captured_stdout, isspace);

  // Expect about:blank page DOM.
  EXPECT_THAT(captured_stdout,
              testing::HasSubstr("<html><head></head><body></body></html>"));
}

class HeadlessModeDumpDomCommandBrowserTestWithSubResourceTimeout
    : public HeadlessModeDumpDomCommandBrowserTestWithTimeoutBase,
      public testing::WithParamInterface<bool> {
 public:
  HeadlessModeDumpDomCommandBrowserTestWithSubResourceTimeout() = default;

  bool delay_response() { return GetParam(); }

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HeadlessModeDumpDomCommandBrowserTestWithSubResourceTimeout::
            RequestHandler,
        base::Unretained(this)));

    HeadlessModeDumpDomCommandBrowserTestWithTimeoutBase::SetUp();
  }

 private:
  std::string GetTargetPage() override { return "/page.html"; }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/page.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(R"(
        <html><body>
        <div id="thediv">INITIAL</div>
        <script src="./script.js"></script>
        </body></html>
      )");

      return response;
    }

    if (request.relative_url == "/script.js") {
      std::unique_ptr<net::test_server::BasicHttpResponse> response;
      if (delay_response()) {
        response = std::make_unique<net::test_server::DelayedHttpResponse>(
            timeout() * 2);
      } else {
        response = std::make_unique<net::test_server::BasicHttpResponse>();
      }

      response->set_code(net::HTTP_OK);
      response->set_content_type("text/javascript");
      response->set_content(R"(
        document.getElementById("thediv").innerText="REPLACED";
      )");

      return response;
    }

    return nullptr;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HeadlessModeDumpDomCommandBrowserTestWithSubResourceTimeout,
    testing::Bool());

IN_PROC_BROWSER_TEST_P(
    HeadlessModeDumpDomCommandBrowserTestWithSubResourceTimeout,
    HeadlessDumpDomWithSubResourceTimeout) {
  absl::optional<HeadlessCommandHandler::Result> result = ProcessCommands();

  capture_stdout_.StopCapture();
  std::string captured_stdout = capture_stdout_.TakeCapturedData();
  std::erase_if(captured_stdout, isspace);

  if (delay_response()) {
    EXPECT_THAT(result,
                testing::Eq(HeadlessCommandHandler::Result::kPageLoadTimeout));
    EXPECT_THAT(captured_stdout,
                testing::HasSubstr(
                    "<html><head></head><body><divid=\"thediv\">INITIAL</"
                    "div><scriptsrc=\"./script.js\"></script></body></html>"));
  } else {
    EXPECT_THAT(result, testing::Eq(HeadlessCommandHandler::Result::kSuccess));
    EXPECT_THAT(captured_stdout,
                testing::HasSubstr(
                    "<html><head></head><body><divid=\"thediv\">REPLACED</"
                    "div><scriptsrc=\"./script.js\"></script></body></html>"));
  }
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
  ASSERT_THAT(ProcessCommands(),
              testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  base::ScopedAllowBlockingForTesting allow_blocking;

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
        temp_dir().Append(FILE_PATH_LITERAL("print-to.pdf"));
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
  ASSERT_THAT(ProcessCommands(),
              testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  base::ScopedAllowBlockingForTesting allow_blocking;

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
  ASSERT_THAT(ProcessCommands(),
              testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string pdf_data;
  ASSERT_TRUE(base::ReadFileToString(print_to_pdf_filename_, &pdf_data))
      << print_to_pdf_filename_;

  PDFPageBitmap page_bitmap;
  ASSERT_TRUE(page_bitmap.Render(pdf_data, /*page_index=*/4));

  // Expect green rectangle on white background.
  EXPECT_TRUE(page_bitmap.CheckColoredRect(SkColorSetRGB(0x00, 0x64, 0x00),
                                           SkColorSetRGB(0xff, 0xff, 0xff)));
}

class HeadlessModeTaggedPrintToPdfCommandBrowserTest
    : public HeadlessModePrintToPdfCommandBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  HeadlessModeTaggedPrintToPdfCommandBrowserTest() = default;

  bool generate_tagged_pdf() { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModePrintToPdfCommandBrowserTestBase::SetUpCommandLine(
        command_line);
    command_line->AppendArg(GetTargetUrl("/hello.html").spec());

    if (!generate_tagged_pdf()) {
      command_line->AppendSwitch(switches::kDisablePDFTagging);
    }
  }
};

const char kExpectedStructTreeJSON[] = R"({
   "lang": "en-US",
   "type": "Document",
   "~children": [ {
      "type": "H1",
      "~children": [ {
         "type": "NonStruct"
      } ]
   } ]
}
)";

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HeadlessModeTaggedPrintToPdfCommandBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(HeadlessModeTaggedPrintToPdfCommandBrowserTest,
                       HeadlessTaggedPrintToPdf) {
  ASSERT_THAT(ProcessCommands(),
              testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string pdf_data;
  ASSERT_TRUE(base::ReadFileToString(print_to_pdf_filename_, &pdf_data))
      << print_to_pdf_filename_;

  auto pdf_span = base::as_bytes(base::make_span(pdf_data));

  int num_pages;
  ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages,
                                        /*max_page_width=*/nullptr));

  EXPECT_THAT(num_pages, testing::Eq(1));

  ASSERT_THAT(chrome_pdf::IsPDFDocTagged(pdf_span),
              testing::Optional(generate_tagged_pdf()));

  if (generate_tagged_pdf()) {
    base::Value struct_tree =
        chrome_pdf::GetPDFStructTreeForPage(pdf_span, /*page_index=*/0);
    EXPECT_THAT(kExpectedStructTreeJSON, base::test::IsJson((struct_tree)));
  }
}

}  // namespace headless
