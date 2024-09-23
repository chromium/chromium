// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64url.h"

#include <string_view>

#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;
using testing::Optional;

namespace base {

namespace {

TEST(Base64UrlTest, BinaryIncludePaddingPolicy) {
  const uint8_t kData[] = {0x00, 0x01, 0xFE, 0xFF};

  std::string binary_encoded_with_padding;
  Base64UrlEncode(kData, Base64UrlEncodePolicy::INCLUDE_PADDING,
                  &binary_encoded_with_padding);

  // Check that encoding the same binary data through the std::string_view
  // interface gives the same result.
  std::string string_encoded_with_padding;
  Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(kData), sizeof(kData)),
      Base64UrlEncodePolicy::INCLUDE_PADDING, &string_encoded_with_padding);
  EXPECT_EQ(binary_encoded_with_padding, string_encoded_with_padding);

  // Check that decoding the result gives the same binary data.
  EXPECT_THAT(Base64UrlDecode(string_encoded_with_padding,
                              Base64UrlDecodePolicy::REQUIRE_PADDING),
              Optional(ElementsAreArray(kData)));

  EXPECT_THAT(Base64UrlDecode(string_encoded_with_padding,
                              Base64UrlDecodePolicy::IGNORE_PADDING),
              Optional(ElementsAreArray(kData)));

  EXPECT_THAT(Base64UrlDecode(string_encoded_with_padding,
                              Base64UrlDecodePolicy::DISALLOW_PADDING),
              std::nullopt);
}

TEST(Base64UrlTest, BinaryOmitPaddingPolicy) {
  const uint8_t kData[] = {0x00, 0x01, 0xFE, 0xFF};

  std::string binary_encoded_without_padding;
  Base64UrlEncode(kData, Base64UrlEncodePolicy::OMIT_PADDING,
                  &binary_encoded_without_padding);

  // Check that encoding the same binary data through the std::string_view
  // interface gives the same result.
  std::string string_encoded_without_padding;
  Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(kData), sizeof(kData)),
      Base64UrlEncodePolicy::OMIT_PADDING, &string_encoded_without_padding);
  EXPECT_EQ(binary_encoded_without_padding, string_encoded_without_padding);

  // Check that decoding the result gives the same binary data.
  EXPECT_THAT(Base64UrlDecode(string_encoded_without_padding,
                              Base64UrlDecodePolicy::DISALLOW_PADDING),
              Optional(ElementsAreArray(kData)));

  EXPECT_THAT(Base64UrlDecode(string_encoded_without_padding,
                              Base64UrlDecodePolicy::IGNORE_PADDING),
              Optional(ElementsAreArray(kData)));

  EXPECT_THAT(Base64UrlDecode(string_encoded_without_padding,
                              Base64UrlDecodePolicy::REQUIRE_PADDING),
              std::nullopt);
}

TEST(Base64UrlTest, EncodeIncludePaddingPolicy) {
  std::string output;
  Base64UrlEncode("hello?world", Base64UrlEncodePolicy::INCLUDE_PADDING,
                  &output);

  // Base64 version: aGVsbG8/d29ybGQ=
  EXPECT_EQ("aGVsbG8_d29ybGQ=", output);

  // Test for behavior for very short and empty strings.
  Base64UrlEncode("??", Base64UrlEncodePolicy::INCLUDE_PADDING, &output);
  EXPECT_EQ("Pz8=", output);

  Base64UrlEncode("", Base64UrlEncodePolicy::INCLUDE_PADDING, &output);
  EXPECT_EQ("", output);
}

TEST(Base64UrlTest, EncodeOmitPaddingPolicy) {
  std::string output;
  Base64UrlEncode("hello?world", Base64UrlEncodePolicy::OMIT_PADDING, &output);

  // base64 version: aGVsbG8/d29ybGQ=
  EXPECT_EQ("aGVsbG8_d29ybGQ", output);

  // Test for behavior for very short and empty strings.
  Base64UrlEncode("??", Base64UrlEncodePolicy::OMIT_PADDING, &output);
  EXPECT_EQ("Pz8", output);

  Base64UrlEncode("", Base64UrlEncodePolicy::OMIT_PADDING, &output);
  EXPECT_EQ("", output);
}

TEST(Base64UrlTest, DecodeRequirePaddingPolicy) {
  std::string output;
  ASSERT_TRUE(Base64UrlDecode("aGVsbG8_d29ybGQ=",
                              Base64UrlDecodePolicy::REQUIRE_PADDING, &output));

  EXPECT_EQ("hello?world", output);

  ASSERT_FALSE(Base64UrlDecode(
      "aGVsbG8_d29ybGQ", Base64UrlDecodePolicy::REQUIRE_PADDING, &output));

  // Test for behavior for very short and empty strings.
  ASSERT_TRUE(
      Base64UrlDecode("Pz8=", Base64UrlDecodePolicy::REQUIRE_PADDING, &output));
  EXPECT_EQ("??", output);

  ASSERT_TRUE(
      Base64UrlDecode("", Base64UrlDecodePolicy::REQUIRE_PADDING, &output));
  EXPECT_EQ("", output);
}

TEST(Base64UrlTest, DecodeIgnorePaddingPolicy) {
  std::string output;
  ASSERT_TRUE(Base64UrlDecode("aGVsbG8_d29ybGQ",
                              Base64UrlDecodePolicy::IGNORE_PADDING, &output));

  EXPECT_EQ("hello?world", output);

  // Including the padding is accepted as well.
  ASSERT_TRUE(Base64UrlDecode("aGVsbG8_d29ybGQ=",
                              Base64UrlDecodePolicy::IGNORE_PADDING, &output));

  EXPECT_EQ("hello?world", output);
}

TEST(Base64UrlTest, DecodeIntoVector) {
  ASSERT_FALSE(
      Base64UrlDecode("invalid=", Base64UrlDecodePolicy::DISALLOW_PADDING));

  static constexpr uint8_t kExpected[] = {'1', '2', '3', '4'};
  std::optional<std::vector<uint8_t>> result =
      Base64UrlDecode("MTIzNA", Base64UrlDecodePolicy::DISALLOW_PADDING);
  ASSERT_TRUE(ranges::equal(*result, kExpected));
}

TEST(Base64UrlTest, DecodeDisallowPaddingPolicy) {
  std::string output;
  ASSERT_FALSE(Base64UrlDecode(
      "aGVsbG8_d29ybGQ=", Base64UrlDecodePolicy::DISALLOW_PADDING, &output));

  // The policy will allow the input when padding has been omitted.
  ASSERT_TRUE(Base64UrlDecode(
      "aGVsbG8_d29ybGQ", Base64UrlDecodePolicy::DISALLOW_PADDING, &output));

  EXPECT_EQ("hello?world", output);
}

TEST(Base64UrlTest, DecodeDisallowsBase64Alphabet) {
  std::string output;

  // The "/" character is part of the conventional base64 alphabet, but has been
  // substituted with "_" in the base64url alphabet.
  ASSERT_FALSE(Base64UrlDecode(
      "aGVsbG8/d29ybGQ=", Base64UrlDecodePolicy::REQUIRE_PADDING, &output));
}

TEST(Base64UrlTest, DecodeDisallowsPaddingOnly) {
  std::string output;

  ASSERT_FALSE(Base64UrlDecode(
      "=", Base64UrlDecodePolicy::IGNORE_PADDING, &output));
  ASSERT_FALSE(Base64UrlDecode(
      "==", Base64UrlDecodePolicy::IGNORE_PADDING, &output));
  ASSERT_FALSE(Base64UrlDecode(
      "===", Base64UrlDecodePolicy::IGNORE_PADDING, &output));
  ASSERT_FALSE(Base64UrlDecode(
      "====", Base64UrlDecodePolicy::IGNORE_PADDING, &output));
}

}  // namespace

}  // namespace base
