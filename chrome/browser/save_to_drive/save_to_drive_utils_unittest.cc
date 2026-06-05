// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_utils.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace save_to_drive {

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_ValidHeader) {
  std::string_view data = "%PDF-1.5 content";
  mojo_base::BigBuffer buffer(data.size());
  UNSAFE_BUFFERS(base::span(buffer.data(), buffer.size()))
      .copy_from(base::as_bytes(base::span(data)));
  EXPECT_TRUE(ValidatePdfMagic(buffer));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_InvalidHeader) {
  std::string_view data = "Not a PDF";
  mojo_base::BigBuffer buffer(data.size());
  UNSAFE_BUFFERS(base::span(buffer.data(), buffer.size()))
      .copy_from(base::as_bytes(base::span(data)));
  EXPECT_FALSE(ValidatePdfMagic(buffer));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_EmptyBuffer) {
  mojo_base::BigBuffer buffer;
  EXPECT_FALSE(ValidatePdfMagic(buffer));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_ShortBufferValid) {
  std::string_view data = "%PDF-";
  mojo_base::BigBuffer buffer(data.size());
  UNSAFE_BUFFERS(base::span(buffer.data(), buffer.size()))
      .copy_from(base::as_bytes(base::span(data)));
  EXPECT_TRUE(ValidatePdfMagic(buffer));
}

TEST(SaveToDriveUtilsTest, ValidatePdfMagic_ShortBufferInvalid) {
  std::string_view data = "%PD";
  mojo_base::BigBuffer buffer(data.size());
  UNSAFE_BUFFERS(base::span(buffer.data(), buffer.size()))
      .copy_from(base::as_bytes(base::span(data)));
  EXPECT_FALSE(ValidatePdfMagic(buffer));
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
