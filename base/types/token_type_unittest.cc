// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/token_type.h"

#include "base/test/gtest_util.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using FooToken = TokenType<class Foo>;
static_assert(std::is_trivially_copy_constructible_v<FooToken>);
static_assert(std::is_trivially_copy_assignable_v<FooToken>);
static_assert(std::is_trivially_move_constructible_v<FooToken>);
static_assert(std::is_trivially_move_assignable_v<FooToken>);
static_assert(std::is_trivially_destructible_v<FooToken>);

TEST(TokenType, TokenApi) {
  // Test default initialization.
  FooToken token1;
  EXPECT_FALSE(token1.value().is_empty());

  // Test copy construction.
  FooToken token2(token1);
  EXPECT_FALSE(token2.value().is_empty());
  EXPECT_EQ(token1.value(), token2.value());

  // Test assignment.
  FooToken token3;
  token3 = token2;
  EXPECT_FALSE(token3.value().is_empty());
  EXPECT_EQ(token2.value(), token3.value());

  FooToken token4;

  // Test comparison operators.
  EXPECT_TRUE(token1 == token2);
  EXPECT_TRUE(token2 == token3);
  EXPECT_TRUE((token4 < token1) ^ (token1 < token4));
  EXPECT_FALSE(token1 != token2);
  EXPECT_TRUE(token1 != token4);

  // Test hasher.
  EXPECT_EQ(FooToken::Hasher()(token2), UnguessableTokenHash()(token2.value()));

  // Test string representation.
  EXPECT_EQ(token2.ToString(), token2.value().ToString());
}

TEST(TokenType, TokenFromNullUnguessableToken) {
  EXPECT_CHECK_DEATH({ FooToken{UnguessableToken::Null()}; });
}

}  // namespace base
