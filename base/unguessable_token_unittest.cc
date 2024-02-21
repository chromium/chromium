// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/unguessable_token.h"

#include <memory>
#include <sstream>
#include <type_traits>

#include "base/hash/hash.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

void TestSmallerThanOperator(const UnguessableToken& a,
                             const UnguessableToken& b) {
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

TEST(UnguessableTokenTest, VerifyEveryBit) {
  std::optional<UnguessableToken> token = UnguessableToken::Deserialize(1, 2);
  ASSERT_TRUE(token.has_value());
  uint64_t high = 1;
  uint64_t low = 2;

  for (uint64_t bit = 1; bit != 0; bit <<= 1) {
    uint64_t new_high = high ^ bit;
    std::optional<UnguessableToken> new_token =
        UnguessableToken::Deserialize(new_high, low);
    ASSERT_TRUE(new_token.has_value());
    EXPECT_FALSE(*token == *new_token);
  }

  for (uint64_t bit = 1; bit != 0; bit <<= 1) {
    uint64_t new_low = low ^ bit;
    std::optional<UnguessableToken> new_token =
        UnguessableToken::Deserialize(high, new_low);
    ASSERT_TRUE(new_token.has_value());
    EXPECT_FALSE(*token == *new_token);
  }
}

TEST(UnguessableTokenTest, VerifyEqualityOperators) {
  // Deserialize is used for testing purposes.
  // Use UnguessableToken::Create() in production code instead.
  UnguessableToken token = UnguessableToken::Deserialize(1, 2).value();
  UnguessableToken same_token = UnguessableToken::Deserialize(1, 2).value();
  UnguessableToken diff_token = UnguessableToken::Deserialize(1, 3).value();
  UnguessableToken empty_token;

  EXPECT_TRUE(token == token);
  EXPECT_FALSE(token != token);

  EXPECT_TRUE(token == same_token);
  EXPECT_FALSE(token != same_token);

  EXPECT_FALSE(token == diff_token);
  EXPECT_FALSE(diff_token == token);
  EXPECT_TRUE(token != diff_token);
  EXPECT_TRUE(diff_token != token);

  EXPECT_TRUE(empty_token == empty_token);
  EXPECT_FALSE(empty_token != empty_token);
  for (const UnguessableToken& this_token : {token, same_token, diff_token}) {
    EXPECT_FALSE(this_token == empty_token);
    EXPECT_TRUE(this_token != empty_token);
  }
}

TEST(UnguessableTokenTest, VerifyConstructors) {
  UnguessableToken token = UnguessableToken::Create();
  EXPECT_FALSE(token.is_empty());
  EXPECT_TRUE(token);

  UnguessableToken copied_token(token);
  EXPECT_TRUE(copied_token);
  EXPECT_EQ(token, copied_token);

  UnguessableToken uninitialized;
  EXPECT_TRUE(uninitialized.is_empty());
  EXPECT_FALSE(uninitialized);

  EXPECT_TRUE(UnguessableToken().is_empty());
  EXPECT_FALSE(UnguessableToken());
}

TEST(UnguessableTokenTest, VerifySerialization) {
  UnguessableToken token = UnguessableToken::Create();

  uint64_t high = token.GetHighForSerialization();
  uint64_t low = token.GetLowForSerialization();

  EXPECT_TRUE(high);
  EXPECT_TRUE(low);

  std::optional<UnguessableToken> Deserialized =
      UnguessableToken::Deserialize(high, low);
  ASSERT_TRUE(Deserialized.has_value());
  EXPECT_EQ(token, *Deserialized);
}

// Common case (~88% of the time) - no leading zeroes in high_ nor low_.
TEST(UnguessableTokenTest, VerifyToString1) {
  UnguessableToken token =
      UnguessableToken::Deserialize(0x1234567890ABCDEF, 0xFEDCBA0987654321)
          .value();
  std::string expected = "1234567890ABCDEFFEDCBA0987654321";

  EXPECT_EQ(expected, token.ToString());

  std::string expected_stream = "(1234567890ABCDEFFEDCBA0987654321)";
  std::stringstream stream;
  stream << token;
  EXPECT_EQ(expected_stream, stream.str());
}

// Less common case - leading zeroes in high_ or low_ (testing with both).
TEST(UnguessableTokenTest, VerifyToString2) {
  UnguessableToken token = UnguessableToken::Deserialize(0x123, 0xABC).value();
  std::string expected = "00000000000001230000000000000ABC";

  EXPECT_EQ(expected, token.ToString());

  std::string expected_stream = "(00000000000001230000000000000ABC)";
  std::stringstream stream;
  stream << token;
  EXPECT_EQ(expected_stream, stream.str());
}

TEST(UnguessableTokenTest, VerifyToStringUniqueness) {
  const UnguessableToken token1 =
      UnguessableToken::Deserialize(0x0000000012345678, 0x0000000123456789)
          .value();
  const UnguessableToken token2 =
      UnguessableToken::Deserialize(0x0000000123456781, 0x0000000023456789)
          .value();
  EXPECT_NE(token1.ToString(), token2.ToString());
}

TEST(UnguessableTokenTest, VerifyDeserializeZeroes) {
  std::optional<UnguessableToken> token = UnguessableToken::Deserialize(0, 0);

  EXPECT_FALSE(token.has_value());
}

TEST(UnguessableTokenTest, VerifyDeserializeFromString) {
  auto expected = UnguessableToken::CreateForTesting(1, 2);
  auto actual = UnguessableToken::DeserializeFromString(
      "00000000000000010000000000000002");
  EXPECT_TRUE(actual.has_value());
  EXPECT_TRUE(actual.value() == expected);
}

TEST(UnguessableTokenTest, VerifyDeserializeFromInvalidString) {
  const char* invalid_representations[] = {
      // Not a hex string representing 128 bits.
      "1234",
      // A string with valid length of 128 bits but 'X' is not a hex value.
      "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
      // A invalid hex string because of the lower case letters.
      "0123456789abcdef0123456789abcdef",
      // A zeroed out token is not a valid `UnguessableToken`.
      "00000000000000000000000000000000"};
  for (auto* invalid_representation : invalid_representations) {
    auto actual =
        UnguessableToken::DeserializeFromString(invalid_representation);
    EXPECT_FALSE(actual.has_value())
        << "'" << invalid_representation
        << "' should not be deserialized to an UnguessableToken.";
    ;
  }
}

TEST(UnguessableTokenTest, VerifySmallerThanOperator) {
  // Deserialize is used for testing purposes.
  // Use UnguessableToken::Create() in production code instead.
  {
    SCOPED_TRACE("a.low < b.low and a.high == b.high.");
    TestSmallerThanOperator(UnguessableToken::Deserialize(0, 1).value(),
                            UnguessableToken::Deserialize(0, 5).value());
  }
  {
    SCOPED_TRACE("a.low == b.low and a.high < b.high.");
    TestSmallerThanOperator(UnguessableToken::Deserialize(1, 0).value(),
                            UnguessableToken::Deserialize(5, 0).value());
  }
  {
    SCOPED_TRACE("a.low < b.low and a.high < b.high.");
    TestSmallerThanOperator(UnguessableToken::Deserialize(1, 1).value(),
                            UnguessableToken::Deserialize(5, 5).value());
  }
  {
    SCOPED_TRACE("a.low > b.low and a.high < b.high.");
    TestSmallerThanOperator(UnguessableToken::Deserialize(1, 10).value(),
                            UnguessableToken::Deserialize(10, 1).value());
  }
}

TEST(UnguessableTokenTest, VerifyHash) {
  UnguessableToken token = UnguessableToken::Create();

  EXPECT_EQ(base::HashInts64(token.GetHighForSerialization(),
                             token.GetLowForSerialization()),
            UnguessableTokenHash()(token));
}

TEST(UnguessableTokenTest, VerifyBasicUniqueness) {
  EXPECT_NE(UnguessableToken::Create(), UnguessableToken::Create());

  UnguessableToken token = UnguessableToken::Create();
  EXPECT_NE(token.GetHighForSerialization(), token.GetLowForSerialization());
}
}
