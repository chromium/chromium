// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_nup_converter_client.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "pdf/pdf.h"
#include "printing/print_settings.h"
#include "ui/gfx/geometry/size_f.h"

namespace printing {

namespace {

base::FilePath GetTestDir() {
  base::FilePath dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &dir);
  if (!dir.empty())
    dir = dir.AppendASCII("printing");
  return dir;
}

base::MappedReadOnlyRegion GetPdfRegion(const char* file_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::MappedReadOnlyRegion pdf_region;

  base::FilePath test_data_dir = GetTestDir();
  if (test_data_dir.empty())
    return pdf_region;

  base::FilePath pdf_file = test_data_dir.AppendASCII(file_name);
  std::string pdf_str;
  if (!base::ReadFileToString(pdf_file, &pdf_str) || pdf_str.empty())
    return pdf_region;

  pdf_region = base::ReadOnlySharedMemoryRegion::Create(pdf_str.size());
  if (!pdf_region.IsValid())
    return pdf_region;

  memcpy(pdf_region.mapping.memory(), pdf_str.data(), pdf_str.size());
  return pdf_region;
}

base::MappedReadOnlyRegion GetBadDataRegion() {
  static const char kBadData[] = "BADDATA";
  base::MappedReadOnlyRegion pdf_region =
      base::ReadOnlySharedMemoryRegion::Create(std::size(kBadData));
  if (!pdf_region.IsValid())
    return pdf_region;

  memcpy(pdf_region.mapping.memory(), kBadData, std::size(kBadData));
  return pdf_region;
}

std::vector<gfx::SizeF> GetPdfPageSizes(base::span<const uint8_t> pdf_data) {
  int num_pages;
  if (!chrome_pdf::GetPDFDocInfo(pdf_data, &num_pages, nullptr) ||
      num_pages <= 0) {
    return {};
  }

  std::vector<gfx::SizeF> sizes;
  for (int i = 0; i < num_pages; ++i) {
    std::optional<gfx::SizeF> page_size =
        chrome_pdf::GetPDFPageSizeByIndex(pdf_data, i);
    if (!page_size.has_value())
      return {};

    sizes.push_back(page_size.value());
  }

  return sizes;
}

void VerifyPdf(base::span<const uint8_t> pdf_data,
               base::span<const gfx::SizeF> expected_sizes) {
  std::vector<gfx::SizeF> page_sizes = GetPdfPageSizes(pdf_data);
  ASSERT_EQ(expected_sizes.size(), page_sizes.size());
  for (size_t i = 0; i < expected_sizes.size(); ++i)
    EXPECT_EQ(expected_sizes[i], page_sizes[i]);
}

base::span<const gfx::SizeF> GetExpectedPdfSizes(std::string_view pdf_name) {
  if (pdf_name == "hello_world.pdf") {
    static constexpr gfx::SizeF kSizes[] = {
        {612.0f, 792.0f},
    };
    return kSizes;
  }
  if (pdf_name == "pdf_converter_basic.pdf") {
    static constexpr gfx::SizeF kSizes[] = {
        {612.0f, 792.0f},
        {612.0f, 792.0f},
        {612.0f, 792.0f},
    };
    return kSizes;
  }

  NOTREACHED();
}

}  // namespace

class PdfNupConverterClientBrowserTest : public InProcessBrowserTest {
 public:
  struct ConvertResult {
    mojom::PdfNupConverter::Status status;
    base::ReadOnlySharedMemoryRegion nup_pdf_region;
  };

  using NupTestFuture =
      base::test::TestFuture<mojom::PdfNupConverter::Status,
                             base::ReadOnlySharedMemoryRegion>;

  PdfNupConverterClientBrowserTest() = default;
  ~PdfNupConverterClientBrowserTest() override = default;

  std::optional<ConvertResult> ConvertDocument(
      base::ReadOnlySharedMemoryRegion pdf_region,
      int pages_per_sheet) {
    auto converter = std::make_unique<PdfNupConverterClient>(
        browser()->tab_strip_model()->GetActiveWebContents());

    NupTestFuture future;
    converter->DoNupPdfDocumentConvert(
        /*document_cookie=*/PrintSettings::NewCookie(), pages_per_sheet,
        /*page_size=*/gfx::Size(612, 792),
        /*printable_area=*/gfx::Rect(612, 792), std::move(pdf_region),
        future.GetCallback());

    if (!future.Wait()) {
      // Give the caller a chance to fail gracefully. Whereas calling
      // `future.Take()` without handling the Wait() failure will result in a
      // CHECK() crash.
      return std::nullopt;
    }

    auto result = future.Take();
    return ConvertResult{std::get<0>(result), std::move(std::get<1>(result))};
  }

  std::optional<ConvertResult> ConvertPages(
      std::vector<base::ReadOnlySharedMemoryRegion> pdf_regions,
      int pages_per_sheet) {
    auto converter = std::make_unique<PdfNupConverterClient>(
        browser()->tab_strip_model()->GetActiveWebContents());

    NupTestFuture future;
    converter->DoNupPdfConvert(
        /*document_cookie=*/PrintSettings::NewCookie(), pages_per_sheet,
        /*page_size=*/gfx::Size(612, 792),
        /*printable_area=*/gfx::Rect(612, 792), std::move(pdf_regions),
        future.GetCallback());

    if (!future.Wait()) {
      // See comment in the `future.Wait()` call in ConvertDocument() above.
      return std::nullopt;
    }

    auto result = future.Take();
    return ConvertResult{std::get<0>(result), std::move(std::get<1>(result))};
  }
};

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest,
                       DocumentConvert2UpSuccess) {
  base::MappedReadOnlyRegion pdf_region =
      GetPdfRegion("pdf_converter_basic.pdf");
  ASSERT_TRUE(pdf_region.IsValid());

  // Make sure pdf_converter_basic.pdf is as expected.
  VerifyPdf(pdf_region.mapping.GetMemoryAsSpan<uint8_t>(),
            GetExpectedPdfSizes("pdf_converter_basic.pdf"));

  std::optional<ConvertResult> result =
      ConvertDocument(std::move(pdf_region.region), /*pages_per_sheet=*/2);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::SUCCESS, result.value().status);
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping =
      result.value().nup_pdf_region.Map();
  ASSERT_TRUE(nup_pdf_mapping.IsValid());

  // For 2-up, a 3 page portrait document fits on 2 landscape-oriented pages.
  static constexpr gfx::SizeF kExpectedSizes[] = {
      {792.0f, 612.0f},
      {792.0f, 612.0f},
  };
  VerifyPdf(nup_pdf_mapping.GetMemoryAsSpan<uint8_t>(), kExpectedSizes);
}

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest,
                       DocumentConvert4UpSuccess) {
  base::MappedReadOnlyRegion pdf_region =
      GetPdfRegion("pdf_converter_basic.pdf");
  ASSERT_TRUE(pdf_region.IsValid());

  // Make sure pdf_converter_basic.pdf is as expected.
  VerifyPdf(pdf_region.mapping.GetMemoryAsSpan<uint8_t>(),
            GetExpectedPdfSizes("pdf_converter_basic.pdf"));

  std::optional<ConvertResult> result =
      ConvertDocument(std::move(pdf_region.region), /*pages_per_sheet=*/4);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::SUCCESS, result.value().status);
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping =
      result.value().nup_pdf_region.Map();
  ASSERT_TRUE(nup_pdf_mapping.IsValid());

  // For 4-up, a 3 page portrait document fits on 1 portrait-oriented page.
  static constexpr gfx::SizeF kExpectedSizes[] = {
      {612.0f, 792.0f},
  };
  VerifyPdf(nup_pdf_mapping.GetMemoryAsSpan<uint8_t>(), kExpectedSizes);
}

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest,
                       DocumentConvertBadData) {
  base::MappedReadOnlyRegion pdf_region = GetBadDataRegion();
  ASSERT_TRUE(pdf_region.IsValid());

  std::optional<ConvertResult> result =
      ConvertDocument(std::move(pdf_region.region), /*pages_per_sheet=*/2);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
            result.value().status);
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping =
      result.value().nup_pdf_region.Map();
  EXPECT_FALSE(nup_pdf_mapping.IsValid());
}

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest,
                       PagesConvert2UpSuccess) {
  std::vector<base::ReadOnlySharedMemoryRegion> pdf_regions;
  {
    base::MappedReadOnlyRegion pdf_region = GetPdfRegion("hello_world.pdf");
    ASSERT_TRUE(pdf_region.IsValid());

    // Make sure hello_world.pdf is as expected.
    VerifyPdf(pdf_region.mapping.GetMemoryAsSpan<uint8_t>(),
              GetExpectedPdfSizes("hello_world.pdf"));

    // Use hello_world.pdf, which only has 1 page, as the 2 pages of input for
    // this N-up operation.
    auto pdf_region_copy = pdf_region.region.Duplicate();
    ASSERT_TRUE(pdf_region_copy.IsValid());
    pdf_regions.push_back(std::move(pdf_region_copy));
    pdf_regions.push_back(std::move(pdf_region.region));
  }

  std::optional<ConvertResult> result =
      ConvertPages(std::move(pdf_regions), /*pages_per_sheet=*/2);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::SUCCESS, result.value().status);
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping =
      result.value().nup_pdf_region.Map();
  ASSERT_TRUE(nup_pdf_mapping.IsValid());

  // For 2-up, a 2 page portrait document fits on 1 landscape-oriented pages.
  static constexpr gfx::SizeF kExpectedSizes[] = {
      {792.0f, 612.0f},
  };
  VerifyPdf(nup_pdf_mapping.GetMemoryAsSpan<uint8_t>(), kExpectedSizes);
}

IN_PROC_BROWSER_TEST_F(PdfNupConverterClientBrowserTest, PagesConvertBadData) {
  std::vector<base::ReadOnlySharedMemoryRegion> pdf_regions;
  {
    base::MappedReadOnlyRegion pdf_region = GetPdfRegion("hello_world.pdf");
    ASSERT_TRUE(pdf_region.IsValid());

    // Make sure hello_world.pdf is as expected.
    VerifyPdf(pdf_region.mapping.GetMemoryAsSpan<uint8_t>(),
              GetExpectedPdfSizes("hello_world.pdf"));

    base::MappedReadOnlyRegion bad_pdf_region = GetBadDataRegion();
    ASSERT_TRUE(bad_pdf_region.IsValid());

    pdf_regions.push_back(std::move(pdf_region.region));
    pdf_regions.push_back(std::move(bad_pdf_region.region));
  }

  std::optional<ConvertResult> result =
      ConvertPages(std::move(pdf_regions), /*pages_per_sheet=*/2);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
            result.value().status);
  base::ReadOnlySharedMemoryMapping nup_pdf_mapping =
      result.value().nup_pdf_region.Map();
  EXPECT_FALSE(nup_pdf_mapping.IsValid());
}

}  // namespace printing
