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

}  // namespace save_to_drive
