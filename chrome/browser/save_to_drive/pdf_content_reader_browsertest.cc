// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/pdf_content_reader.h"

#include <memory>
#include <string_view>

#include "base/containers/span.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace save_to_drive {
namespace {

void VerifyMagicString(base::span<const uint8_t> block) {
  constexpr std::string_view kExpectedMagicString = "%PDF";
  std::string_view file_content = base::as_string_view(block);
  ASSERT_GE(file_content.size(), kExpectedMagicString.size());
  const std::string_view actual_magic_string =
      file_content.substr(0, kExpectedMagicString.size());
  EXPECT_EQ(actual_magic_string, kExpectedMagicString);
}

}  // namespace

class PDFContentReaderBrowserTest : public base::test::WithFeatureOverride,
                                    public PDFExtensionTestBase {
 public:
  PDFContentReaderBrowserTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  PDFContentReaderBrowserTest(const PDFContentReaderBrowserTest&) = delete;
  PDFContentReaderBrowserTest& operator=(const PDFContentReaderBrowserTest&) =
      delete;

  ~PDFContentReaderBrowserTest() override = default;

  bool UseOopif() const override { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(PDFContentReaderBrowserTest, ReadFullPDF) {
  GURL page_url = chrome_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("pdf/accessibility")),
      base::FilePath(FILE_PATH_LITERAL("hello-world-in-image.pdf")));
  auto* extension_frame = LoadPdfGetExtensionHost(page_url);
  ASSERT_TRUE(extension_frame);

  auto content_reader = std::make_unique<PDFContentReader>(
      extension_frame, pdf::mojom::SaveRequestType::kOriginal);

  base::test::TestFuture<bool> open_future;
  content_reader->Open(open_future.GetCallback());
  ASSERT_TRUE(open_future.Get());

  constexpr size_t kExpectedSize = 9106u;
  EXPECT_EQ(content_reader->GetSize(), kExpectedSize);

  base::test::TestFuture<mojo_base::BigBuffer> read_future;
  content_reader->Read(0, kExpectedSize, read_future.GetCallback());
  mojo_base::BigBuffer block = read_future.Take();
  EXPECT_EQ(block.size(), kExpectedSize);
  VerifyMagicString(base::span(block));
}

IN_PROC_BROWSER_TEST_P(PDFContentReaderBrowserTest, ReadInChunks) {
  GURL page_url = chrome_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("pdf/accessibility")),
      base::FilePath(FILE_PATH_LITERAL("hello-world-in-image.pdf")));
  auto* extension_frame = LoadPdfGetExtensionHost(page_url);
  ASSERT_TRUE(extension_frame);

  auto content_reader = std::make_unique<PDFContentReader>(
      extension_frame, pdf::mojom::SaveRequestType::kOriginal);

  base::test::TestFuture<bool> open_future;
  content_reader->Open(open_future.GetCallback());
  ASSERT_TRUE(open_future.Get());

  constexpr size_t kTotalSize = 9106u;
  EXPECT_EQ(content_reader->GetSize(), kTotalSize);
  constexpr size_t kChunkSize = 1000u;

  for (size_t offset = 0; offset < kTotalSize; offset += kChunkSize) {
    base::test::TestFuture<mojo_base::BigBuffer> read_future;
    const size_t chunk_size = std::min(kTotalSize - offset, kChunkSize);
    content_reader->Read(offset, chunk_size, read_future.GetCallback());
    mojo_base::BigBuffer block = read_future.Take();
    EXPECT_EQ(block.size(), chunk_size);
    if (offset == 0) {
      VerifyMagicString(base::span(block));
    }
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFContentReaderBrowserTest);

}  // namespace save_to_drive
