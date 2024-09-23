// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/headless/test/pdf_utils.h"
#include "components/printing/browser/print_manager_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "pdf/pdf.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

using DevToolsProtocolTest = DevToolsProtocolTestBase;

namespace {

class PrintToPdfProtocolTest : public DevToolsProtocolTest,
                               public testing::WithParamInterface<bool> {
 protected:
  static constexpr double kPaperWidth = 10;
  static constexpr double kPaperHeight = 15;
  static constexpr int kDpi = headless::PDFPageBitmap::kDpi;

  bool headless() const { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTest::SetUpCommandLine(command_line);
    if (headless())
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
    const std::string& data = *result()->FindString("data");
    ASSERT_TRUE(base::Base64Decode(data, &pdf_data_));

    pdf_span_ = base::as_bytes(base::make_span(pdf_data_));

    ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span_, &pdf_num_pages_, nullptr));
    ASSERT_GE(pdf_num_pages_, 1);
  }

  void CreatePdfSpanFromResultStream() {
    std::string stream = *result()->FindString("stream");
    ASSERT_GT(stream.length(), 0ul);

    pdf_data_.clear();
    for (;;) {
      base::Value::Dict params;
      params.Set("handle", stream);
      params.Set("offset", static_cast<int>(pdf_data_.size()));
      const base::Value::Dict* result =
          SendCommandSync("IO.read", std::move(params));
      std::string data = *result->FindString("data");
      if (result->FindBool("base64Encoded").value_or(false))
        ASSERT_TRUE(base::Base64Decode(data, &data));
      pdf_data_.append(std::move(data));
      if (result->FindBool("eof").value_or(false))
        break;
    }

    pdf_span_ = base::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(pdf_data_.data()), pdf_data_.size());

    ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span_, &pdf_num_pages_, nullptr));
    ASSERT_GE(pdf_num_pages_, 1);
  }

  void PrintToPdf(base::Value::Dict params) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultData();
  }

  void PrintToPdfAsStream(base::Value::Dict params) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultStream();
  }

  void PrintToPdfAndRenderPage(base::Value::Dict params, int page_index) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultData();
    ASSERT_TRUE(page_bitmap.Render(pdf_span_, page_index));
  }

  void PrintToPdfAsStreamAndRenderPage(base::Value::Dict params,
                                       int page_index) {
    SendCommandSync("Page.printToPDF", std::move(params));
    CreatePdfSpanFromResultStream();
    ASSERT_TRUE(page_bitmap.Render(pdf_span_, page_index));
  }

  uint32_t GetPixelRGB(int x, int y) { return page_bitmap.GetPixelRGB(x, y); }

  int bitmap_width() { return page_bitmap.width(); }
  int bitmap_height() { return page_bitmap.height(); }

  net::EmbeddedTestServer https_server_;

  std::string pdf_data_;
  base::raw_span<const uint8_t, DanglingUntriaged> pdf_span_;
  int pdf_num_pages_ = 0;

  headless::PDFPageBitmap page_bitmap;
};

INSTANTIATE_TEST_SUITE_P(HeadfulOrHeadless,
                         PrintToPdfProtocolTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, PrintToPdfBackground) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", 0);
  params.Set("marginLeft", 0);
  params.Set("marginBottom", 0);
  params.Set("marginRight", 0);

  PrintToPdfAndRenderPage(std::move(params), 0);

  // Expect top left pixel of background color
  EXPECT_EQ(GetPixelRGB(0, 0), 0x123456u);

  // Expect midpoint pixel of red color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 2, bitmap_height() / 2), 0xff0000u);
}

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, PrintToPdfMargins) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", 1.0);
  params.Set("marginLeft", 1.0);
  params.Set("marginBottom", 0);
  params.Set("marginRight", 0);

  PrintToPdfAndRenderPage(std::move(params), 0);

  // Expect top left pixel of white color
  EXPECT_EQ(GetPixelRGB(0, 0), 0xffffffu);

  // Expect pixel at a quarter of diagonal of background color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 4, bitmap_height() / 4), 0x123456u);

  // Expect midpoint pixel of red color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 2, bitmap_height() / 2), 0xff0000u);
}

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, PrintToPdfHeaderFooter) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();

  constexpr double kHeaderMargin = 1.0;
  constexpr double kFooterMargin = 1.0;

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", kHeaderMargin);
  params.Set("marginLeft", 0);
  params.Set("marginBottom", kFooterMargin);
  params.Set("marginRight", 0);
  params.Set("displayHeaderFooter", true);
  params.Set("headerTemplate",
             "<div style='height: 1cm; width: 1cm; background: "
             "#00ff00; -webkit-print-color-adjust: exact;'>");
  params.Set("footerTemplate",
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
    base::Value::Dict params;
    params.Set("printBackground", true);
    params.Set("paperWidth", kPaperWidth);
    params.Set("paperHeight", kPaperHeight);
    params.Set("marginTop", 0);
    params.Set("marginLeft", 0);
    params.Set("marginBottom", 0);
    params.Set("marginRight", 0);
    params.Set("scale", scale);

    PrintToPdfAndRenderPage(std::move(params), 0);

    int y = bitmap_height() / 2;
    uint32_t start_clr = GetPixelRGB(0, y);
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

INSTANTIATE_TEST_SUITE_P(HeadfulOrHeadless,
                         PrintToPdfScaleTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(PrintToPdfScaleTest, PrintToPdfScaleArea) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();

  constexpr double kScaleFactor = 2.0;
  constexpr double kDefaultScaleFactor = 1.0;
  int scaled_red_square_width = RenderAndReturnRedSquareWidth(kScaleFactor);
  int unscaled_red_square_width =
      RenderAndReturnRedSquareWidth(kDefaultScaleFactor);

  EXPECT_EQ(unscaled_red_square_width * kScaleFactor, scaled_red_square_width);
}

class PrintToPdfPaperOrientationTest : public PrintToPdfProtocolTest {
 protected:
  std::optional<gfx::SizeF> PrintToPdfAndReturnPageSize(
      bool landscape = false) {
    base::Value::Dict params;
    params.Set("paperWidth", kPaperWidth);
    params.Set("paperHeight", kPaperHeight);
    params.Set("landscape", landscape);

    PrintToPdf(std::move(params));

    return chrome_pdf::GetPDFPageSizeByIndex(pdf_span_, 0);
  }
};

INSTANTIATE_TEST_SUITE_P(HeadfulOrHeadless,
                         PrintToPdfPaperOrientationTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(PrintToPdfPaperOrientationTest,
                       PrintToPdfPaperOrientation) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();

  std::optional<gfx::SizeF> portrait_page_size = PrintToPdfAndReturnPageSize();
  ASSERT_TRUE(portrait_page_size.has_value());
  EXPECT_GT(portrait_page_size->height(), portrait_page_size->width());

  std::optional<gfx::SizeF> landscape_page_size =
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
    base::Value::Dict params;
    params.Set("expression", height_expression);

    SendCommandSync("Runtime.evaluate", std::move(params));
  }

  base::Value::Dict BuildPrintParams(const std::string& page_ranges) {
    base::Value::Dict params;
    params.Set("paperWidth", kPaperWidth);
    params.Set("paperHeight", kPaperHeight);
    params.Set("marginTop", 0);
    params.Set("marginLeft", 0);
    params.Set("marginBottom", 0);
    params.Set("marginRight", 0);
    params.Set("pageRanges", page_ranges);
    return params;
  }

  void PrintPageRanges(const std::string& page_ranges) {
    PrintToPdf(BuildPrintParams(page_ranges));
  }
};

INSTANTIATE_TEST_SUITE_P(HeadfulOrHeadless,
                         PrintToPdfPagesTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(PrintToPdfPagesTest, PrintToPdfPageRanges) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();
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

  // Expect specific error for ranges beyond end of the document.
  SendCommand("Page.printToPDF", BuildPrintParams("998-999"));

  EXPECT_THAT(*error()->FindString("message"),
              testing::Eq("Page range exceeds page count"));
}

IN_PROC_BROWSER_TEST_P(PrintToPdfPagesTest, PrintToPdfCssPageSize) {
  NavigateToURLBlockUntilNavigationsComplete(
      "/print_to_pdf/css_page_size.html");

  Attach();
  SetDocHeight();

  base::Value::Dict params;
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("preferCSSPageSize", true);

  PrintToPdf(std::move(params));

  // Css page size in /css_page_size.html is smaller than requested
  // paper size, so expect greater page count.
  const int kExpectedTotalPages = std::ceil(kDocHeight / kPaperHeight);
  EXPECT_GT(pdf_num_pages_, kExpectedTotalPages);
}

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, PrintToPdfAsStream) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", 0);
  params.Set("marginLeft", 0);
  params.Set("marginBottom", 0);
  params.Set("marginRight", 0);
  params.Set("transferMode", "ReturnAsStream");

  PrintToPdfAsStreamAndRenderPage(std::move(params), 0);

  // Expect top left pixel of background color
  EXPECT_EQ(GetPixelRGB(0, 0), 0x123456u);

  // Expect midpoint pixel of red color
  EXPECT_EQ(GetPixelRGB(bitmap_width() / 2, bitmap_height() / 2), 0xff0000u);
}

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, HasDocumentOutline) {
  NavigateToURLBlockUntilNavigationsComplete(
      "/print_to_pdf/structured_doc.html");

  Attach();

  base::Value::Dict params;
  // generating a document outline at the moment requires a tagged pdf
  params.Set("generateTaggedPDF", true);
  params.Set("generateDocumentOutline", true);
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", 0);
  params.Set("marginLeft", 0);
  params.Set("marginBottom", 0);
  params.Set("marginRight", 0);
  params.Set("transferMode", "ReturnAsStream");

  PrintToPdfAsStream(std::move(params));

  std::optional<bool> has_outline = chrome_pdf::PDFDocHasOutline(pdf_span_);
  EXPECT_THAT(has_outline, testing::Optional(true));
}

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, Title) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/basic.html");

  Attach();

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", 0);
  params.Set("marginLeft", 0);
  params.Set("marginBottom", 0);
  params.Set("marginRight", 0);
  params.Set("transferMode", "ReturnAsStream");

  PrintToPdfAsStream(std::move(params));

  std::optional<chrome_pdf::DocumentMetadata> metadata =
      chrome_pdf::GetPDFDocMetadata(pdf_span_);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->title, "PrintToPdf Basic Test");
}

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, PrintToPdfOOPIF) {
  NavigateToURLBlockUntilNavigationsComplete("/print_to_pdf/oopif.html");

  Attach();

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", 0);
  params.Set("marginLeft", 0);
  params.Set("marginBottom", 0);
  params.Set("marginRight", 0);
  PrintToPdfAndRenderPage(std::move(params), 0);

  ASSERT_TRUE(printing::IsOopifEnabled());

  // Expect red iframe pixel at 1 inch into the page.
  EXPECT_EQ(GetPixelRGB(1 * kDpi, 1 * kDpi), 0xff0000u);
}

IN_PROC_BROWSER_TEST_P(PrintToPdfProtocolTest, JpegCmykIccPrintToPdf) {
  NavigateToURLBlockUntilNavigationsComplete(
      "/print_to_pdf/red-cmyk-turned-green-via-icc_profile.html");

  Attach();

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("paperWidth", kPaperWidth);
  params.Set("paperHeight", kPaperHeight);
  params.Set("marginTop", 0);
  params.Set("marginLeft", 0);
  params.Set("marginBottom", 0);
  params.Set("marginRight", 0);
  PrintToPdfAndRenderPage(std::move(params), 0);

  ASSERT_TRUE(printing::IsOopifEnabled());

  // These color values have been transformed from CMYK+icc to XYZ to sRGB+icc
  // to displayRGB using several different color managers. Many approximations
  // and rounding have been applied along the way. So carefully test for a
  // green-ish rectangle.
  constexpr SkColor background = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);

  // Find the first non-background color pixel.
  int x = 0;
  int y = 0;
  auto find_box = [&x, &y, this]() -> bool {
    const int width = bitmap_width();
    const int height = bitmap_height();
    for (x = 0; x < width; ++x) {
      for (y = 0; y < height; ++y) {
        SkColor c = SkColorSetA(GetPixelRGB(x, y), 0xFF);
        if (c != background) {
          return true;
        }
      }
    }
    return false;
  };
  ASSERT_TRUE(find_box());

  // Sample a pixel color in the expected rectangle.
  SkColor c = SkColorSetA(GetPixelRGB(x + 10, y + 10), 0xFF);
  uint32_t r = SkColorGetR(c);
  uint32_t g = SkColorGetG(c);
  uint32_t b = SkColorGetB(c);

  // Expect that it is green-ish.
  EXPECT_LT(r, 0x10u);
  EXPECT_GT(g, 0xF0u);
  EXPECT_LT(b, 0x10u);

  // Expect green rectangle on white background.
  EXPECT_TRUE(page_bitmap.CheckColoredRect(c, background));
}

}  // namespace
