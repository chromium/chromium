// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/base64.h"

#include <string_view>

#include "base/numerics/checked_math.h"
#include "base/strings/escape.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/modp_b64/modp_b64.h"

namespace base {

TEST(Base64Test, Basic) {
  const std::string kText = "hello world";
  const std::string kBase64Text = "aGVsbG8gd29ybGQ=";

  std::string decoded;
  bool ok;

  std::string encoded = Base64Encode(kText);
  EXPECT_EQ(kBase64Text, encoded);

  ok = Base64Decode(encoded, &decoded);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kText, decoded);
}

TEST(Base64Test, Forgiving) {
  struct {
    const char* in;

    // nullptr indicates a decode failure.
    const char* expected_out;
  } kTestCases[] = {
      // Failures that should apply in all decoding modes:
      //
      // - Characters not in the base64 alphabet
      {"abc&", nullptr},
      {"ab-d", nullptr},
      // - input len % 4 == 1
      {"abcde", nullptr},
      {"a", nullptr},

      // Invalid padding causes failure if kForgiving is set.
      {"abcd=", nullptr},
      {"abcd==", nullptr},
      {"abcd===", nullptr},
      {"abcd====", nullptr},
      {"abcd==============", nullptr},
      {"=", nullptr},
      {"====", nullptr},

      // Otherwise, inputs that are multiples of 4 always succeed, this matches
      // kStrict mode.
      {"abcd", "i\xB7\x1D"},
      {"abc=", "i\xB7"},
      {"abcdefgh", "i\xB7\x1Dy\xF8!"},

      // kForgiving mode allows for omitting padding (to a multiple of 4) if
      // len % 4 != 1.
      {"abcdef", "i\xB7\x1Dy"},
      {"abc", "i\xB7"},
      {"ab", "i"},

      // Whitespace should be allowed if kForgiving is set, matching
      // https://infra.spec.whatwg.org/#ascii-whitespace:
      // ASCII whitespace is U+0009 TAB '\t', U+000A LF '\n', U+000C FF '\f',
      // U+000D CR '\r', or U+0020 SPACE ' '.
      {" a bcd", "i\xB7\x1D"},
      {"ab\t\tc=", "i\xB7"},
      {"ab c\ndefgh", "i\xB7\x1Dy\xF8!"},
      {"a\tb\nc\f d\r", "i\xB7\x1D"},

      // U+000B VT '\v' is _not_ valid whitespace to be stripped.
      {"ab\vcd", nullptr},

      // Empty string should yield an empty result.
      {"", ""},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << EscapeAllExceptUnreserved(test_case.in));
    std::string output;
    bool success =
        Base64Decode(test_case.in, &output, Base64DecodePolicy::kForgiving);
    bool expected_success = test_case.expected_out != nullptr;
    EXPECT_EQ(success, expected_success);
    if (expected_success) {
      EXPECT_EQ(output, test_case.expected_out);
    }
  }
}

TEST(Base64Test, Binary) {
  const uint8_t kData[] = {0x00, 0x01, 0xFE, 0xFF};

  std::string binary_encoded = Base64Encode(kData);

  // Check that encoding the same data through the std::string_view interface
  // gives the same results.
  std::string string_piece_encoded = Base64Encode(
      std::string_view(reinterpret_cast<const char*>(kData), sizeof(kData)));

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

  std::string text = Base64Encode(kText);
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

  // `modp_b64_encode_data_len` is a macro, so check `MODP_B64_MAX_INPUT_LEN` is
  // correct be verifying the computation doesn't overflow.
  base::CheckedNumeric<size_t> max_len = MODP_B64_MAX_INPUT_LEN;
  EXPECT_TRUE(modp_b64_encode_data_len(max_len).IsValid());
}

}  // namespace base
