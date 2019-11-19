// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_to_emf_converter.h"

#include <windows.h>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "printing/emf_win.h"
#include "printing/metafile.h"
#include "printing/pdf_render_settings.h"

namespace printing {

namespace {

constexpr gfx::Rect kLetter200DpiRect = gfx::Rect(0, 0, 1700, 2200);
constexpr gfx::Size k200DpiSize = gfx::Size(200, 200);

constexpr size_t kHeaderSize = sizeof(ENHMETAHEADER);

void StartCallbackImpl(base::OnceClosure quit_closure,
                       int* page_count_out,
                       int page_count_in) {
  *page_count_out = page_count_in;
  std::move(quit_closure).Run();
}

void GetPageCallbackImpl(base::OnceClosure quit_closure,
                         int* page_number_out,
                         std::unique_ptr<MetafilePlayer>* file_out,
                         int page_number_in,
                         float scale_factor,
                         std::unique_ptr<MetafilePlayer> file_in) {
  *page_number_out = page_number_in;
  *file_out = std::move(file_in);
  std::move(quit_closure).Run();
}

// |page_number| is 0-based. Returned result has 1-based page number.
std::string GetFileNameForPageNumber(const std::string& name, int page_number) {
  std::string ret = name;
  ret += std::to_string(page_number + 1);
  ret += ".emf";
  return ret;
}

std::unique_ptr<ENHMETAHEADER> GetEmfHeader(const std::string& emf_data) {
  Emf emf;
  if (!emf.InitFromData(emf_data.data(), emf_data.size()))
    return nullptr;

  auto meta_header = std::make_unique<ENHMETAHEADER>();
  if (GetEnhMetaFileHeader(emf.emf(), kHeaderSize, meta_header.get()) !=
      kHeaderSize) {
    return nullptr;
  }
  return meta_header;
}

void CompareEmfHeaders(const ENHMETAHEADER& expected_header,
                       const ENHMETAHEADER& actual_header) {
  // TODO(crbug.com/781403): once the EMF generation is fixed, also compare:
  //  rclBounds, rclFrame, szlDevice, szlMillimeters and szlMicrometers.
  EXPECT_EQ(expected_header.iType, actual_header.iType);
  EXPECT_EQ(expected_header.nSize, actual_header.nSize);
  EXPECT_EQ(expected_header.dSignature, actual_header.dSignature);
  EXPECT_EQ(expected_header.nVersion, actual_header.nVersion);
  EXPECT_EQ(expected_header.nBytes, actual_header.nBytes);
  EXPECT_EQ(expected_header.nRecords, actual_header.nRecords);
  EXPECT_EQ(expected_header.nHandles, actual_header.nHandles);
  EXPECT_EQ(expected_header.sReserved, actual_header.sReserved);
  EXPECT_EQ(expected_header.nDescription, actual_header.nDescription);
  EXPECT_EQ(expected_header.offDescription, actual_header.offDescription);
  EXPECT_EQ(expected_header.nPalEntries, actual_header.nPalEntries);
  EXPECT_EQ(expected_header.cbPixelFormat, actual_header.cbPixelFormat);
  EXPECT_EQ(expected_header.offPixelFormat, actual_header.offPixelFormat);
  EXPECT_EQ(expected_header.bOpenGL, actual_header.bOpenGL);
}

std::string HashData(const char* data, size_t len) {
  auto span = base::make_span(reinterpret_cast<const uint8_t*>(data), len);
  return base::HexEncode(base::SHA1HashSpan(span));
}

class PdfToEmfConverterBrowserTest : public InProcessBrowserTest {
 protected:
  PdfToEmfConverterBrowserTest() : test_data_dir_(GetTestDataDir()) {}
  ~PdfToEmfConverterBrowserTest() override = default;

  void RunSinglePagePdfToPostScriptConverterTest(
      const PdfRenderSettings& pdf_settings,
      base::StringPiece input_filename,
      base::StringPiece output_filename) {
    ASSERT_TRUE(GetTestInput(input_filename));
    ASSERT_TRUE(StartPdfConverter(pdf_settings, 1));
    ASSERT_TRUE(GetPage(0));
    // The output is PS encapsulated in EMF.
    ASSERT_TRUE(GetPageExpectedEmfData(output_filename));
    ComparePageEmfHeader();
    ComparePageEmfPayload();
  }

  bool GetTestInput(base::StringPiece filename) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    if (test_data_dir_.empty())
      return false;

    base::FilePath pdf_file = test_data_dir_.AppendASCII(filename);
    std::string pdf_data_str;
    if (!base::ReadFileToString(pdf_file, &pdf_data_str))
      return false;

    if (pdf_data_str.empty())
      return false;

    test_input_ = base::RefCountedString::TakeString(&pdf_data_str);
    return true;
  }

  bool StartPdfConverter(const PdfRenderSettings& pdf_settings,
                         int expected_page_count) {
    base::RunLoop run_loop;
    int page_count = -1;
    pdf_converter_ = PdfConverter::StartPdfConverter(
        test_input_, pdf_settings,
        base::BindOnce(&StartCallbackImpl, run_loop.QuitClosure(),
                       &page_count));
    run_loop.Run();
    return pdf_converter_ && (expected_page_count == page_count);
  }

  bool GetPage(int page_number_in) {
    base::RunLoop run_loop;
    int page_number = -1;
    pdf_converter_->GetPage(
        page_number_in,
        base::BindRepeating(&GetPageCallbackImpl, run_loop.QuitClosure(),
                            &page_number, &current_emf_file_));
    run_loop.Run();

    if (!current_emf_file_ || (page_number_in != page_number))
      return false;

    return GetEmfData();
  }

  bool GetPageExpectedEmfData(base::StringPiece filename) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath emf_file = test_data_dir_.AppendASCII(filename);
    return base::ReadFileToString(emf_file, &expected_current_emf_data_) &&
           !expected_current_emf_data_.empty();
  }

  void ComparePageEmfHeader() {
    // TODO(crbug.com/781403): the generated data can differ visually. Until
    // this is fixed only checking the output size and parts of the EMF header.
    ASSERT_EQ(expected_current_emf_data_.size(),
              actual_current_emf_data_.size());

    std::unique_ptr<ENHMETAHEADER> expected_header =
        GetEmfHeader(expected_current_emf_data_);
    ASSERT_TRUE(expected_header);
    std::unique_ptr<ENHMETAHEADER> actual_header =
        GetEmfHeader(actual_current_emf_data_);
    ASSERT_TRUE(actual_header);
    CompareEmfHeaders(*expected_header, *actual_header);
  }

  void ComparePageEmfPayload() {
    ASSERT_EQ(expected_current_emf_data_.size(),
              actual_current_emf_data_.size());
    ASSERT_GT(expected_current_emf_data_.size(), kHeaderSize);
    size_t size = expected_current_emf_data_.size() - kHeaderSize;
    EXPECT_EQ(HashData(expected_current_emf_data_.data() + kHeaderSize, size),
              HashData(actual_current_emf_data_.data() + kHeaderSize, size));
  }

 private:
  base::FilePath GetTestDataDir() const {
    base::FilePath test_data_dir;
    if (base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir))
      test_data_dir = test_data_dir.AppendASCII("printing");
    return test_data_dir;
  }

  bool GetEmfData() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::vector<char> buffer;
    if (!current_emf_file_->GetDataAsVector(&buffer))
      return false;

    actual_current_emf_data_.assign(buffer.data(), buffer.size());
    return !actual_current_emf_data_.empty();
  }

  const base::FilePath test_data_dir_;
  scoped_refptr<base::RefCountedString> test_input_;
  std::unique_ptr<PdfConverter> pdf_converter_;

  std::unique_ptr<MetafilePlayer> current_emf_file_;
  std::string expected_current_emf_data_;
  std::string actual_current_emf_data_;

  DISALLOW_COPY_AND_ASSIGN(PdfToEmfConverterBrowserTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest, FailureNoTempFile) {
  ScopedSimulateFailureCreatingTempFileForTests fail_creating_temp_file;

  base::RunLoop run_loop;
  int page_count = -1;
  std::unique_ptr<PdfConverter> pdf_converter = PdfConverter::StartPdfConverter(
      base::MakeRefCounted<base::RefCountedStaticMemory>(), PdfRenderSettings(),
      base::BindOnce(&StartCallbackImpl, run_loop.QuitClosure(), &page_count));
  run_loop.Run();
  EXPECT_EQ(0, page_count);
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest, FailureBadPdf) {
  scoped_refptr<base::RefCountedStaticMemory> bad_pdf_data =
      base::MakeRefCounted<base::RefCountedStaticMemory>("0123456789", 10);

  base::RunLoop run_loop;
  int page_count = -1;
  std::unique_ptr<PdfConverter> pdf_converter = PdfConverter::StartPdfConverter(
      bad_pdf_data, PdfRenderSettings(),
      base::BindOnce(&StartCallbackImpl, run_loop.QuitClosure(), &page_count));
  run_loop.Run();
  EXPECT_EQ(0, page_count);
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest, EmfBasic) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false,
      /*use_color=*/true, PdfRenderSettings::Mode::NORMAL);
  constexpr int kNumberOfPages = 3;

  ASSERT_TRUE(GetTestInput("pdf_converter_basic.pdf"));
  ASSERT_TRUE(StartPdfConverter(pdf_settings, kNumberOfPages));
  for (int i = 0; i < kNumberOfPages; ++i) {
    ASSERT_TRUE(GetPage(i));
    ASSERT_TRUE(GetPageExpectedEmfData(
        GetFileNameForPageNumber("pdf_converter_basic_emf_page_", i)));
    ComparePageEmfHeader();
    // TODO(thestig): Check if ComparePageEmfPayload() works on bots.
  }
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest, PostScriptLevel2Basic) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/true,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2);
  constexpr int kNumberOfPages = 3;

  ASSERT_TRUE(GetTestInput("pdf_converter_basic.pdf"));
  ASSERT_TRUE(StartPdfConverter(pdf_settings, kNumberOfPages));
  for (int i = 0; i < kNumberOfPages; ++i) {
    ASSERT_TRUE(GetPage(i));
    // The output is PS encapsulated in EMF.
    ASSERT_TRUE(GetPageExpectedEmfData(
        GetFileNameForPageNumber("pdf_converter_basic_ps_page_", i)));
    ComparePageEmfHeader();
    ComparePageEmfPayload();
  }
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest, PostScriptLevel3Basic) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/true,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3);
  constexpr int kNumberOfPages = 3;

  ASSERT_TRUE(GetTestInput("pdf_converter_basic.pdf"));
  ASSERT_TRUE(StartPdfConverter(pdf_settings, kNumberOfPages));
  for (int i = 0; i < kNumberOfPages; ++i) {
    ASSERT_TRUE(GetPage(i));
    // The output is PS encapsulated in EMF.
    ASSERT_TRUE(GetPageExpectedEmfData(
        GetFileNameForPageNumber("pdf_converter_basic_ps_page_", i)));
    ComparePageEmfHeader();
    ComparePageEmfPayload();
  }
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest, PostScriptLevel2Mono) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/false,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2);
  RunSinglePagePdfToPostScriptConverterTest(pdf_settings, "bug_767343.pdf",
                                            "bug_767343_mono.emf");
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest, PostScriptLevel3Mono) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/false,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3);
  RunSinglePagePdfToPostScriptConverterTest(pdf_settings, "bug_767343.pdf",
                                            "bug_767343_mono.emf");
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest,
                       PostScriptLevel2WithZeroSizedText) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/true,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2);
  RunSinglePagePdfToPostScriptConverterTest(pdf_settings, "bug_767343.pdf",
                                            "bug_767343.emf");
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest,
                       PostScriptLevel3WithZeroSizedText) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/true,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3);
  RunSinglePagePdfToPostScriptConverterTest(pdf_settings, "bug_767343.pdf",
                                            "bug_767343.emf");
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest,
                       PostScriptLevel2WithNegativeSizedText) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/true,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2);
  RunSinglePagePdfToPostScriptConverterTest(pdf_settings, "bug_806746.pdf",
                                            "bug_806746.emf");
}

IN_PROC_BROWSER_TEST_F(PdfToEmfConverterBrowserTest,
                       PostScriptLevel3WithNegativeSizedText) {
  const PdfRenderSettings pdf_settings(
      kLetter200DpiRect, gfx::Point(0, 0), k200DpiSize,
      /*autorotate=*/false, /*use_color=*/true,
      PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3);
  RunSinglePagePdfToPostScriptConverterTest(pdf_settings, "bug_806746.pdf",
                                            "bug_806746.emf");
}

}  // namespace printing
