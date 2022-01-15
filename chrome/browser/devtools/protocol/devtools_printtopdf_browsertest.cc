// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// Native headless is currently available only on Linux platform. More
// platforms will be added soon, so avoid function level clutter by
// ifdefing the entire file.
#if BUILDFLAG(IS_LINUX)

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "pdf/pdf.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using DevToolsProtocolTest = DevToolsProtocolTestBase;

namespace {

class PrintToPdfProtocolTest : public DevToolsProtocolTest {
 protected:
  static constexpr double kPaperWidth = 10;
  static constexpr double kPaperHeight = 15;
  static constexpr int kColorChannels = 4;
  static constexpr int kDpi = 300;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(crbug.com/1240796): Page.printToPdf is currently available only when
    // Chrome is running in headless mode.
    DevToolsProtocolTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("headless", "chrome");
  }

  void PreRunTestOnMainThread() override {
    DevToolsProtocolTest::PreRunTestOnMainThread();
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void NavigateToURLBlockUntilNavigationsComplete(const std::string& url) {
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), https_server_.GetURL(url), 1);
    content::NavigationEntry* entry =
        web_contents()->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(entry);
  }

  void CreatePdfSpanFromResultData() {
    auto* data = result_.FindKeyOfType("data", base::Value::Type::STRING);
    ASSERT_TRUE(data);
    ASSERT_TRUE(base::Base64Decode(data->GetString(), &pdf_data_));

    pdf_span_ = base::as_bytes(base::make_span(pdf_data_));

    ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span_, &pdf_num_pages_, nullptr));
    ASSERT_GE(pdf_num_pages_, 1);
  }

  void CreatePdfSpanFromResultStream() {
    ASSERT_NE(result_.FindKeyOfType("stream", base::Value::Type::STRING),
              nullptr);
    const std::string stream = result_.FindKey("stream")->GetString();
    ASSERT_GT(stream.length(), 0ul);

    std::string data;
    for (;;) {
      base::Value params(base::Value::Type::DICTIONARY);
      params.SetStringPath("handle", stream);
      params.SetIntPath("offset", pdf_data_.size());
      SendCommandSync("IO.read", std::move(params));
      data.append(result_.FindKey("data")->GetString());
      if (result_.FindKeyOfType("eof", base::Value::Type::BOOLEAN))
        break;
    }

    absl::optional<bool> base64Encoded = result_.FindBoolPath("base64Encoded");
    if (base64Encoded && *base64Encoded) {
      ASSERT_TRUE(base::Base64Decode(data, &pdf_data_));
    } else {
      pdf_data_ = std::move(data);
    }

    pdf_span_ = base::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(pdf_data_.data()), pdf_data_.size());

    ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span_, &pdf_num_pages_, nullptr));
    ASSERT_GE(pdf_num_pages_, 1);
  }

  void PrintToPdf(base::Value params) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultData();
  }

  void PrintToPdfAsStream(base::Value params) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultStream();
  }

  void PrintToPdfAndRenderPage(base::Value params, int page_index) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultData();
    RendePdfPage(page_index);
  }

  void PrintToPdfAsStreamAndRenderPage(base::Value params, int page_index) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultStream();
    RendePdfPage(page_index);
  }

  void RendePdfPage(int page_index) {
    constexpr chrome_pdf::RenderOptions options = {
        .stretch_to_bounds = false,
        .keep_aspect_ratio = true,
        .autorotate = true,
        .use_color = true,
        .render_device_type = chrome_pdf::RenderDeviceType::kPrinter,
    };

    absl::optional<gfx::SizeF> page_size =
        chrome_pdf::GetPDFPageSizeByIndex(pdf_span_, page_index);
    ASSERT_TRUE(page_size.has_value());

    gfx::Rect rect(kPaperWidth * kDpi, kPaperHeight * kDpi);
    printing::PdfRenderSettings settings(
        rect, gfx::Point(), gfx::Size(kDpi, kDpi), options.autorotate,
        options.use_color, printing::PdfRenderSettings::Mode::NORMAL);
    std::vector<uint8_t> bitmap_data(kColorChannels *
                                     settings.area.size().GetArea());
    ASSERT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
        pdf_span_, page_index, bitmap_data.data(), settings.area.size(),
        settings.dpi, options));

    bitmap_data_.swap(bitmap_data);
    bitmap_size_ = settings.area.size();
  }

  uint32_t GetPixelRGB(int x, int y) {
    int pixel_index =
        bitmap_size_.width() * y * kColorChannels + x * kColorChannels;
    return bitmap_data_[pixel_index + 0]           // B
           | bitmap_data_[pixel_index + 1] << 8    // G
           | bitmap_data_[pixel_index + 2] << 16;  // R
  }

  int bitmap_width() { return bitmap_size_.width(); }
  int bitmap_height() { return bitmap_size_.height(); }

  net::EmbeddedTestServer https_server_;

  std::string pdf_data_;
  base::span<const uint8_t> pdf_span_;
  int pdf_num_pages_ = 0;

  std::vector<uint8_t> bitmap_data_;
  gfx::Size bitmap_size_;
};

IN_PROC_BROWSER_TEST_F(PrintToPdfProtocolTest, PrintToPdfBackground) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
  SendCommand("Page.enable");

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetBoolPath("printBackground", true);
  params.SetDoublePath("paperWidth", kPaperWidth);
  params.SetDoublePath("paperHeight", kPaperHeight);
  params.SetDoublePath("marginTop", 0);
  params.SetDoublePath("marginLeft", 0);
  params.SetDoublePath("marginBottom", 0);
  params.SetDoublePath("marginRight", 0);

  PrintToPdfAndRenderPage(std::move(params), 0);

  // Expect top left pixel of background color
  EXPECT_EQ(GetPixelRGB(0, 0), 0x123456u);

  // Expect midpoint pixel of red color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 2, bitmap_height() / 2), 0xff0000u);
}

IN_PROC_BROWSER_TEST_F(PrintToPdfProtocolTest, PrintToPdfMargins) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
  SendCommand("Page.enable");

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetBoolPath("printBackground", true);
  params.SetDoublePath("paperWidth", kPaperWidth);
  params.SetDoublePath("paperHeight", kPaperHeight);
  params.SetDoublePath("marginTop", 1.0);
  params.SetDoublePath("marginLeft", 1.0);
  params.SetDoublePath("marginBottom", 0);
  params.SetDoublePath("marginRight", 0);

  PrintToPdfAndRenderPage(std::move(params), 0);

  // Expect top left pixel of white color
  EXPECT_EQ(GetPixelRGB(0, 0), 0xffffffu);

  // Expect pixel at a quarter of diagonal of background color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 4, bitmap_height() / 4), 0x123456u);

  // Expect midpoint pixel of red color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 2, bitmap_height() / 2), 0xff0000u);
}

IN_PROC_BROWSER_TEST_F(PrintToPdfProtocolTest, PrintToPdfHeaderFooter) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
  SendCommand("Page.enable");

  constexpr double kHeaderMargin = 1.0;
  constexpr double kFooterMargin = 1.0;

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetBoolPath("printBackground", true);
  params.SetDoublePath("paperWidth", kPaperWidth);
  params.SetDoublePath("paperHeight", kPaperHeight);
  params.SetDoublePath("marginTop", kHeaderMargin);
  params.SetDoublePath("marginLeft", 0);
  params.SetDoublePath("marginBottom", kFooterMargin);
  params.SetDoublePath("marginRight", 0);
  params.SetBoolPath("displayHeaderFooter", true);
  params.SetStringPath("headerTemplate",
                       "<div style='height: 1cm; width: 1cm; background: "
                       "#00ff00; -webkit-print-color-adjust: exact;'>");
  params.SetStringPath("footerTemplate",
                       "<div style='height: 1cm; width: 1cm; background: "
                       "#0000ff; -webkit-print-color-adjust: exact;'>");

  PrintToPdfAndRenderPage(std::move(params), 0);

  // Expect top left pixel of white color
  EXPECT_EQ(GetPixelRGB(0, 0), 0xffffffu);

  // Expect header left pixel of green color
  EXPECT_EQ(GetPixelRGB(0, kDpi * kHeaderMargin / 2), 0x00ff00u);

  // Expect footer left pixel of blue color
  EXPECT_EQ(GetPixelRGB(0, bitmap_height() - kDpi * kFooterMargin / 2),
            0x0000ffu);

  // Expect bottom left pixel of white color
  EXPECT_EQ(GetPixelRGB(0, bitmap_height() - 1), 0xffffffu);

  // Expect midpoint pixel of red color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 2, bitmap_height() / 2), 0xff0000u);
}

class PrintToPdfScaleTest : public PrintToPdfProtocolTest {
 protected:
  int RenderAndReturnRedSquareWidth(double scale) {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetBoolPath("printBackground", true);
    params.SetDoublePath("paperWidth", kPaperWidth);
    params.SetDoublePath("paperHeight", kPaperHeight);
    params.SetDoublePath("marginTop", 0);
    params.SetDoublePath("marginLeft", 0);
    params.SetDoublePath("marginBottom", 0);
    params.SetDoublePath("marginRight", 0);
    params.SetDoublePath("scale", scale);

    PrintToPdfAndRenderPage(std::move(params), 0);

    int x = 0;
    int y = bitmap_height() / 2;
    uint32_t start_clr = GetPixelRGB(x, y);
    EXPECT_EQ(start_clr, 0x123456u);

    int red_square_width = 0;
    for (int x = 1; x < bitmap_width(); x++) {
      uint32_t clr = GetPixelRGB(x, y);
      if (clr != start_clr) {
        EXPECT_EQ(clr, 0xff0000u);
        ++red_square_width;
      }
    }

    return red_square_width;
  }
};

IN_PROC_BROWSER_TEST_F(PrintToPdfScaleTest, PrintToPdfScaleArea) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
  SendCommand("Page.enable");

  constexpr double kScaleFactor = 2.0;
  constexpr double kDefaultScaleFactor = 1.0;
  int scaled_red_square_width = RenderAndReturnRedSquareWidth(kScaleFactor);
  int unscaled_red_square_width =
      RenderAndReturnRedSquareWidth(kDefaultScaleFactor);

  EXPECT_EQ(unscaled_red_square_width * kScaleFactor, scaled_red_square_width);
}

class PrintToPdfPaperOrientationTest : public PrintToPdfProtocolTest {
 protected:
  absl::optional<gfx::SizeF> PrintToPdfAndReturnPageSize(
      bool landscape = false) {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetDoublePath("paperWidth", kPaperWidth);
    params.SetDoublePath("paperHeight", kPaperHeight);
    params.SetBoolPath("landscape", landscape);

    PrintToPdf(std::move(params));

    return chrome_pdf::GetPDFPageSizeByIndex(pdf_span_, 0);
  }
};

IN_PROC_BROWSER_TEST_F(PrintToPdfPaperOrientationTest,
                       PrintToPdfPaperOrientation) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
  SendCommand("Page.enable");

  absl::optional<gfx::SizeF> portrait_page_size = PrintToPdfAndReturnPageSize();
  ASSERT_TRUE(portrait_page_size.has_value());
  EXPECT_GT(portrait_page_size->height(), portrait_page_size->width());

  absl::optional<gfx::SizeF> landscape_page_size =
      PrintToPdfAndReturnPageSize(/*landscape=*/true);
  ASSERT_TRUE(landscape_page_size.has_value());
  EXPECT_GT(landscape_page_size->width(), landscape_page_size->height());
}

class PrintToPdfPagesTest : public PrintToPdfProtocolTest {
 protected:
  static constexpr double kDocHeight = 50;

  void SetDocHeight() {
    std::string height_expression = "document.body.style.height = '" +
                                    base::NumberToString(kDocHeight) + "in'";
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetStringPath("expression", height_expression);

    SendCommandSync("Runtime.evaluate", std::move(params));
  }

  void PrintPageRanges(const std::string& page_ranges,
                       bool ignore_invalid_page_ranges = false) {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetDoublePath("paperWidth", kPaperWidth);
    params.SetDoublePath("paperHeight", kPaperHeight);
    params.SetDoublePath("marginTop", 0);
    params.SetDoublePath("marginLeft", 0);
    params.SetDoublePath("marginBottom", 0);
    params.SetDoublePath("marginRight", 0);
    params.SetStringPath("pageRanges", page_ranges);
    params.SetBoolPath("ignoreInvalidPageRanges", ignore_invalid_page_ranges);

    PrintToPdf(std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(PrintToPdfPagesTest, PrintToPdfPageRanges) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
  SendCommand("Page.enable");
  SendCommand("Runtime.enable");
  SetDocHeight();

  const int kExpectedTotalPages = std::ceil(kDocHeight / kPaperHeight);

  // Empty page range prints all pages.
  PrintPageRanges("");
  EXPECT_EQ(pdf_num_pages_, kExpectedTotalPages);

  // Print one page.
  PrintPageRanges("1");
  EXPECT_EQ(pdf_num_pages_, 1);

  // Print list of pages.
  PrintPageRanges("1,2");
  EXPECT_EQ(pdf_num_pages_, 2);

  // Print range of pages.
  PrintPageRanges("1-3");
  EXPECT_EQ(pdf_num_pages_, 3);

  // Open page range is OK.
  PrintPageRanges("-");
  EXPECT_EQ(pdf_num_pages_, kExpectedTotalPages);

  // End page beyond number of pages is OK.
  PrintPageRanges("2-999");
  EXPECT_EQ(pdf_num_pages_, kExpectedTotalPages - 1);

  // Invalid page ranges are OK if explicitly requested.
  PrintPageRanges("998-999", /*ignore_invalid_page_ranges=*/true);
  EXPECT_EQ(pdf_num_pages_, kExpectedTotalPages);
}

IN_PROC_BROWSER_TEST_F(PrintToPdfPagesTest, PrintToPdfCssPageSize) {
  NavigateToURLBlockUntilNavigationsComplete(
      "/print_to_pdf/css_page_size.html");

  Attach();
  SendCommand("Page.enable");
  SendCommand("Runtime.enable");
  SetDocHeight();

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetDoublePath("paperWidth", kPaperWidth);
  params.SetDoublePath("paperHeight", kPaperHeight);
  params.SetBoolPath("preferCSSPageSize", true);

  PrintToPdf(std::move(params));

  // Css page size in /css_page_size.html is smaller than requested
  // paper size, so expect greater page count.
  const int kExpectedTotalPages = std::ceil(kDocHeight / kPaperHeight);
  EXPECT_GT(pdf_num_pages_, kExpectedTotalPages);
}

IN_PROC_BROWSER_TEST_F(PrintToPdfProtocolTest, PrintToPdfAsStream) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
  SendCommand("Page.enable");

  base::Value params(base::Value::Type::DICTIONARY);
  params.SetBoolPath("printBackground", true);
  params.SetDoublePath("paperWidth", kPaperWidth);
  params.SetDoublePath("paperHeight", kPaperHeight);
  params.SetDoublePath("marginTop", 0);
  params.SetDoublePath("marginLeft", 0);
  params.SetDoublePath("marginBottom", 0);
  params.SetDoublePath("marginRight", 0);
  params.SetStringPath("transferMode", "ReturnAsStream");

  PrintToPdfAsStreamAndRenderPage(std::move(params), 0);

  // Expect top left pixel of background color
  EXPECT_EQ(GetPixelRGB(0, 0), 0x123456u);

  // Expect midpoint pixel of red color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 2, bitmap_height() / 2), 0xff0000u);
}

}  // namespace

#endif  // BUILDFLAG(IS_LINUX)
