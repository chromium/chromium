// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;

const uint32_t kTestMaxContextToken = 10u;

class AITextSessionTest : public testing::Test {
 public:
  AITextSession::Context context_{kTestMaxContextToken};
};

// Tests `GetContextString()` and `HasContextItem()` when the context is empty.
TEST_F(AITextSessionTest, TestContextOperation_Empty) {
  EXPECT_EQ(context_.GetContextString(), "");
  EXPECT_FALSE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when some items are added
// to the context.
TEST_F(AITextSessionTest, TestContextOperation_NonEmpty) {
  context_.AddContextItem("test", 1u);
  EXPECT_EQ(context_.GetContextString(), "test");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem(" test again", 2u);
  EXPECT_EQ(context_.GetContextString(), "test test again");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_F(AITextSessionTest, TestContextOperation_Overflow) {
  context_.AddContextItem("test", 1u);
  EXPECT_EQ(context_.GetContextString(), "test");
  EXPECT_TRUE(context_.HasContextItem());

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  context_.AddContextItem("test long token", kTestMaxContextToken);
  EXPECT_EQ(context_.GetContextString(), "test long token");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow on
// the first insertion.
TEST_F(AITextSessionTest, TestContextOperation_OverflowOnFirstItem) {
  context_.AddContextItem("test long long token", kTestMaxContextToken + 1u);
  EXPECT_EQ(context_.GetContextString(), "");
  EXPECT_FALSE(context_.HasContextItem());
}
