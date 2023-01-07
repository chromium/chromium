// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include "base/numerics/checked_math.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/modp_b64/modp_b64.h"

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

  std::string binary_encoded = Base64Encode(kData);

  // Check that encoding the same data through the StringPiece interface gives
  // the same results.
  std::string string_piece_encoded;
  Base64Encode(StringPiece(reinterpret_cast<const char*>(kData), sizeof(kData)),
               &string_piece_encoded);

  EXPECT_EQ(binary_encoded, string_piece_encoded);

  EXPECT_THAT(Base64Decode(binary_encoded),
              testing::Optional(testing::ElementsAreArray(kData)));
  EXPECT_FALSE(Base64Decode("invalid base64!"));

  std::string encoded_with_prefix = "PREFIX";
  Base64EncodeAppend(kData, &encoded_with_prefix);
  EXPECT_EQ(encoded_with_prefix, "PREFIX" + binary_encoded);
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

TEST(Base64Test, Overflow) {
  // `Base64Encode` makes the input larger, which means inputs whose base64
  // output overflows `size_t`. Actually allocating a span of this size will
  // likely fail, but we test it with a fake span and assume a correct
  // implementation will check for overflow before touching the input.
  //
  // Note that, with or without an overflow check, the function will still
  // crash. This test is only meaningful because `EXPECT_CHECK_DEATH` looks for
  // a `CHECK`-based failure.
  uint8_t b;
  auto large_span = base::make_span(&b, MODP_B64_MAX_INPUT_LEN + 1);
  EXPECT_CHECK_DEATH(Base64Encode(large_span));

  std::string output = "PREFIX";
  EXPECT_CHECK_DEATH(Base64EncodeAppend(large_span, &output));

  // `modp_b64_encode_len` is a macro, so check `MODP_B64_MAX_INPUT_LEN` is
  // correct be verifying the computation doesn't overflow.
  base::CheckedNumeric<size_t> max_len = MODP_B64_MAX_INPUT_LEN;
  EXPECT_TRUE(modp_b64_encode_len(max_len).IsValid());
}

}  // namespace base
