// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pwg_raster_converter.h"

#include <optional>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "printing/mojom/print.mojom.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"

namespace printing {

namespace {

constexpr char kPdfToPwgRasterColorTestFile[] = "pdf_to_pwg_raster_test.pwg";
constexpr char kPdfToPwgRasterMonoTestFile[] =
    "pdf_to_pwg_raster_mono_test.pwg";
constexpr char kPdfToPwgRasterLongEdgeTestFile[] =
    "pdf_to_pwg_raster_long_edge_test.pwg";

void ResultCallbackImpl(bool* called,
                        base::ReadOnlySharedMemoryRegion* pwg_region_out,
                        base::OnceClosure quit_closure,
                        base::ReadOnlySharedMemoryRegion pwg_region_in) {
  *called = true;
  *pwg_region_out = std::move(pwg_region_in);
  std::move(quit_closure).Run();
}

void GetPdfData(const char* file_name,
                base::FilePath* test_data_dir,
                scoped_refptr<base::RefCountedString>* pdf_data) {
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, test_data_dir));
  *test_data_dir = test_data_dir->AppendASCII("printing");
  base::FilePath pdf_file = test_data_dir->AppendASCII(file_name);
  std::string pdf_data_str;
  ASSERT_TRUE(base::ReadFileToString(pdf_file, &pdf_data_str));
  ASSERT_GT(pdf_data_str.length(), 0U);
  *pdf_data =
      base::MakeRefCounted<base::RefCountedString>(std::move(pdf_data_str));
}

std::string HashData(base::span<const uint8_t> data) {
  return base::HexEncode(base::SHA1Hash(data));
}

void ComparePwgOutput(const base::FilePath& expected_file,
                      base::ReadOnlySharedMemoryRegion pwg_region) {
  std::string pwg_expected_data_str;
  ASSERT_TRUE(base::ReadFileToString(expected_file, &pwg_expected_data_str));

  base::ReadOnlySharedMemoryMapping pwg_mapping = pwg_region.Map();
  ASSERT_TRUE(pwg_mapping.IsValid());
  size_t size = pwg_mapping.size();
  ASSERT_EQ(pwg_expected_data_str.length(), size);
  EXPECT_EQ(HashData(base::as_byte_span(pwg_expected_data_str)),
            HashData(pwg_mapping.GetMemoryAsSpan<uint8_t>()));
}

class PdfToPwgRasterBrowserTest : public InProcessBrowserTest {
 public:
  PdfToPwgRasterBrowserTest()
      : converter_(PwgRasterConverter::CreateDefault()) {}
  ~PdfToPwgRasterBrowserTest() override {}

  void Convert(const base::RefCountedMemory* pdf_data,
               const PdfRenderSettings& conversion_settings,
               const PwgRasterSettings& bitmap_settings,
               bool expect_success,
               base::ReadOnlySharedMemoryRegion* pwg_region) {
    bool called = false;
    base::RunLoop run_loop;
    converter_->Start(/*use_skia=*/std::nullopt, pdf_data, conversion_settings,
                      bitmap_settings,
                      base::BindOnce(&ResultCallbackImpl, &called, pwg_region,
                                     run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(called);
    EXPECT_EQ(expect_success, pwg_region->IsValid());
  }

 private:
  std::unique_ptr<PwgRasterConverter> converter_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PdfToPwgRasterBrowserTest, TestFailure) {
  scoped_refptr<base::RefCountedStaticMemory> bad_pdf_data =
      base::MakeRefCounted<base::RefCountedStaticMemory>(
          base::byte_span_from_cstring("0123456789"));
  base::ReadOnlySharedMemoryRegion pwg_region;
  Convert(bad_pdf_data.get(), PdfRenderSettings(), PwgRasterSettings(),
          /*expect_success=*/false, &pwg_region);
}

IN_PROC_BROWSER_TEST_F(PdfToPwgRasterBrowserTest, TestSuccessColor) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  scoped_refptr<base::RefCountedString> pdf_data;
  GetPdfData("pdf_to_pwg_raster_test.pdf", &test_data_dir, &pdf_data);

  PdfRenderSettings pdf_settings(gfx::Rect(0, 0, 612, 792), gfx::Point(0, 0),
                                 /*dpi=*/gfx::Size(1000, 1000),
                                 /*autorotate=*/false,
                                 /*use_color=*/true,
                                 PdfRenderSettings::Mode::NORMAL);
  PwgRasterSettings pwg_settings;
  pwg_settings.duplex_mode = mojom::DuplexMode::kSimplex;
  pwg_settings.odd_page_transform = PwgRasterTransformType::TRANSFORM_NORMAL;
  pwg_settings.rotate_all_pages = false;
  pwg_settings.reverse_page_order = false;
  pwg_settings.use_color = true;

  base::ReadOnlySharedMemoryRegion pwg_region;
  Convert(pdf_data.get(), pdf_settings, pwg_settings,
          /*expect_success=*/true, &pwg_region);
  base::FilePath expected_pwg_file =
      test_data_dir.AppendASCII(kPdfToPwgRasterColorTestFile);
  ComparePwgOutput(expected_pwg_file, std::move(pwg_region));
}

IN_PROC_BROWSER_TEST_F(PdfToPwgRasterBrowserTest, TestSuccessMono) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  scoped_refptr<base::RefCountedString> pdf_data;
  GetPdfData("pdf_to_pwg_raster_test.pdf", &test_data_dir, &pdf_data);

  PdfRenderSettings pdf_settings(gfx::Rect(0, 0, 612, 792), gfx::Point(0, 0),
                                 /*dpi=*/gfx::Size(1000, 1000),
                                 /*autorotate=*/false,
                                 /*use_color=*/false,
                                 PdfRenderSettings::Mode::NORMAL);
  PwgRasterSettings pwg_settings;
  pwg_settings.duplex_mode = mojom::DuplexMode::kSimplex;
  pwg_settings.odd_page_transform = PwgRasterTransformType::TRANSFORM_NORMAL;
  pwg_settings.rotate_all_pages = false;
  pwg_settings.reverse_page_order = false;
  pwg_settings.use_color = false;

  base::ReadOnlySharedMemoryRegion pwg_region;
  Convert(pdf_data.get(), pdf_settings, pwg_settings,
          /*expect_success=*/true, &pwg_region);
  base::FilePath expected_pwg_file =
      test_data_dir.AppendASCII(kPdfToPwgRasterMonoTestFile);
  ComparePwgOutput(expected_pwg_file, std::move(pwg_region));
}

IN_PROC_BROWSER_TEST_F(PdfToPwgRasterBrowserTest, TestSuccessLongDuplex) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  scoped_refptr<base::RefCountedString> pdf_data;
  GetPdfData("pdf_to_pwg_raster_test.pdf", &test_data_dir, &pdf_data);

  PdfRenderSettings pdf_settings(gfx::Rect(0, 0, 612, 792), gfx::Point(0, 0),
                                 /*dpi=*/gfx::Size(1000, 1000),
                                 /*autorotate=*/false,
                                 /*use_color=*/false,
                                 PdfRenderSettings::Mode::NORMAL);
  PwgRasterSettings pwg_settings;
  pwg_settings.duplex_mode = mojom::DuplexMode::kLongEdge;
  pwg_settings.odd_page_transform = PwgRasterTransformType::TRANSFORM_NORMAL;
  pwg_settings.rotate_all_pages = false;
  pwg_settings.reverse_page_order = false;
  pwg_settings.use_color = false;

  base::ReadOnlySharedMemoryRegion pwg_region;
  Convert(pdf_data.get(), pdf_settings, pwg_settings,
          /*expect_success=*/true, &pwg_region);
  base::FilePath expected_pwg_file =
      test_data_dir.AppendASCII(kPdfToPwgRasterLongEdgeTestFile);
  ComparePwgOutput(expected_pwg_file, std::move(pwg_region));
}

}  // namespace printing
