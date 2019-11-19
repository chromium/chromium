// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/token.h"

#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Verifies that we can support constexpr Token construction.
constexpr Token kTestToken{1234, 5678};

TEST(TokenTest, Constructors) {
  Token zero_token;
  EXPECT_EQ(0u, zero_token.high());
  EXPECT_EQ(0u, zero_token.low());
  EXPECT_TRUE(zero_token.is_zero());

  Token token_with_explicit_values{1234, 5678};
  EXPECT_EQ(1234u, token_with_explicit_values.high());
  EXPECT_EQ(5678u, token_with_explicit_values.low());
  EXPECT_FALSE(token_with_explicit_values.is_zero());

  Token random_token = Token::CreateRandom();
  EXPECT_FALSE(random_token.is_zero());

  EXPECT_EQ(1234u, kTestToken.high());
  EXPECT_EQ(5678u, kTestToken.low());
  EXPECT_FALSE(kTestToken.is_zero());
}

TEST(TokenTest, Equality) {
  EXPECT_EQ(Token(), Token(0, 0));
  EXPECT_EQ(Token(0, 0), Token(0, 0));
  EXPECT_EQ(Token(1, 2), Token(1, 2));
  EXPECT_NE(Token(1, 2), Token(1, 3));
  EXPECT_NE(Token(1, 2), Token(2, 2));
  EXPECT_NE(Token(1, 2), Token(3, 4));
}

TEST(TokenTest, Ordering) {
  EXPECT_LT(Token(0, 0), Token(0, 1));
  EXPECT_LT(Token(0, 1), Token(0, 2));
  EXPECT_LT(Token(0, 1), Token(1, 0));
  EXPECT_LT(Token(0, 2), Token(1, 0));
}

TEST(TokenTest, ToString) {
  EXPECT_EQ("00000000000000000000000000000000", Token(0, 0).ToString());
  EXPECT_EQ("00000000000000010000000000000002", Token(1, 2).ToString());
  EXPECT_EQ("0123456789ABCDEF5A5A5A5AA5A5A5A5",
            Token(0x0123456789abcdefull, 0x5a5a5a5aa5a5a5a5ull).ToString());
  EXPECT_EQ("FFFFFFFFFFFFFFFDFFFFFFFFFFFFFFFE",
            Token(0xfffffffffffffffdull, 0xfffffffffffffffeull).ToString());
}

TEST(TokenTest, Pickle) {
  Pickle pickle;
  WriteTokenToPickle(&pickle, kTestToken);

  PickleIterator iterator(pickle);
  EXPECT_EQ(kTestToken, ReadTokenFromPickle(&iterator));
}

}  // namespace base
