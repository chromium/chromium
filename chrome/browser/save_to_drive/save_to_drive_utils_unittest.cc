// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_utils.h"

#include <string>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace save_to_drive {

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_ValidHeader) {
  EXPECT_TRUE(
      ValidatePdfMagic(base::byte_span_from_cstring("%PDF-1.5 content")));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_InvalidHeader) {
  EXPECT_FALSE(ValidatePdfMagic(base::byte_span_from_cstring("Not a PDF")));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_EmptyBuffer) {
  EXPECT_FALSE(ValidatePdfMagic({}));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_ShortBufferValid) {
  EXPECT_TRUE(ValidatePdfMagic(base::byte_span_from_cstring("%PDF-")));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_ShortBufferInvalid) {
  EXPECT_FALSE(ValidatePdfMagic(base::byte_span_from_cstring("%PD")));
}

TEST(SaveToDriveUtilsTest, ValidatePdfEof_Valid) {
  EXPECT_TRUE(
      ValidatePdfEof(base::byte_span_from_cstring("startxref\n123\n%%EOF")));
  EXPECT_TRUE(
      ValidatePdfEof(base::byte_span_from_cstring("startxref\n123\n%%EOF\n")));
  EXPECT_TRUE(ValidatePdfEof(
      base::byte_span_from_cstring("startxref\n123\n%%EOF\r\n")));
}

TEST(SaveToDriveUtilsTest, ValidatePdfEof_Invalid) {
  EXPECT_FALSE(ValidatePdfEof({}));
  EXPECT_FALSE(ValidatePdfEof(base::byte_span_from_cstring("not a trailer")));
  EXPECT_FALSE(ValidatePdfEof(base::byte_span_from_cstring("%%EO")));
}

TEST(SaveToDriveUtilsTest, ValidatePdfEof_WithinSearchWindow) {
  // The marker is accepted anywhere within the trailing search window.
  std::string data = "%%EOF" + std::string(512, ' ');
  EXPECT_TRUE(ValidatePdfEof(base::as_byte_span(data)));
}

TEST(SaveToDriveUtilsTest, ValidatePdfEof_OutsideSearchWindow) {
  // A marker that lies before the trailing search window is rejected.
  std::string data = "%%EOF" + std::string(2048, ' ');
  EXPECT_FALSE(ValidatePdfEof(base::as_byte_span(data)));
}

TEST(SaveToDriveUtilsTest, EnsurePdfExtension) {
  // Test case 1: Simple title without extension
  EXPECT_EQ(EnsurePdfExtension(u"My Document"), u"My Document.pdf");

  // Test case 2: Title with .pdf extension (should keep it)
  EXPECT_EQ(EnsurePdfExtension(u"My Document.pdf"), u"My Document.pdf");

  // Test case 3: Title with other extension (should replace it)
  EXPECT_EQ(EnsurePdfExtension(u"My Document.docx"), u"My Document.pdf");

  // Test case 4: Title with multiple extensions (should replace the last one)
  EXPECT_EQ(EnsurePdfExtension(u"My.Document.docx"), u"My.Document.pdf");

  // Test case 5: Empty title (should return empty string as per FilePath
  // behavior)
  EXPECT_EQ(EnsurePdfExtension(u""), u"");

  // Test case 6: Title with spaces and special characters
  EXPECT_EQ(EnsurePdfExtension(u"My Document (1)"), u"My Document (1).pdf");
}

}  // namespace save_to_drive
