// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(Base64Test, Basic) {
  const std::string kText = "hello world";
  const std::string kBase64Text = "aGVsbG8gd29ybGQ=";

  std::string encoded;
  std::string decoded;
  bool ok;

  Base64Encode(kText, &encoded);
  EXPECT_EQ(kBase64Text, encoded);

  ok = Base64Decode(encoded, &decoded);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kText, decoded);
}

TEST(Base64Test, Binary) {
  const uint8_t kData[] = {0x00, 0x01, 0xFE, 0xFF};

  std::string binary_encoded = Base64Encode(make_span(kData));

  // Check that encoding the same data through the StringPiece interface gives
  // the same results.
  std::string string_piece_encoded;
  Base64Encode(StringPiece(reinterpret_cast<const char*>(kData), sizeof(kData)),
               &string_piece_encoded);

  EXPECT_EQ(binary_encoded, string_piece_encoded);
}

TEST(Base64Test, InPlace) {
  const std::string kText = "hello world";
  const std::string kBase64Text = "aGVsbG8gd29ybGQ=";
  std::string text(kText);

  Base64Encode(text, &text);
  EXPECT_EQ(kBase64Text, text);

  bool ok = Base64Decode(text, &text);
  EXPECT_TRUE(ok);
  EXPECT_EQ(text, kText);
}

}  // namespace base
