// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_token.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SequenceTokenTest, IsValid) {
  EXPECT_FALSE(SequenceToken().IsValid());
  EXPECT_TRUE(SequenceToken::Create().IsValid());
}

TEST(SequenceTokenTest, OperatorEquals) {
  const SequenceToken invalid_a;
  const SequenceToken invalid_b;
  const SequenceToken valid_a = SequenceToken::Create();
  const SequenceToken valid_b = SequenceToken::Create();

  EXPECT_FALSE(invalid_a == invalid_a);
  EXPECT_FALSE(invalid_a == invalid_b);
  EXPECT_FALSE(invalid_a == valid_a);
  EXPECT_FALSE(invalid_a == valid_b);

  EXPECT_FALSE(valid_a == invalid_a);
  EXPECT_FALSE(valid_a == invalid_b);
  EXPECT_EQ(valid_a, valid_a);
  EXPECT_FALSE(valid_a == valid_b);
}

TEST(SequenceTokenTest, OperatorNotEquals) {
  const SequenceToken invalid_a;
  const SequenceToken invalid_b;
  const SequenceToken valid_a = SequenceToken::Create();
  const SequenceToken valid_b = SequenceToken::Create();

  EXPECT_NE(invalid_a, invalid_a);
  EXPECT_NE(invalid_a, invalid_b);
  EXPECT_NE(invalid_a, valid_a);
  EXPECT_NE(invalid_a, valid_b);

  EXPECT_NE(valid_a, invalid_a);
  EXPECT_NE(valid_a, invalid_b);
  EXPECT_FALSE(valid_a != valid_a);
  EXPECT_NE(valid_a, valid_b);
}

TEST(SequenceTokenTest, GetForCurrentThread) {
  const SequenceToken token = SequenceToken::Create();

  EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());

  {
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(token);
    EXPECT_TRUE(SequenceToken::GetForCurrentThread().IsValid());
    EXPECT_EQ(token, SequenceToken::GetForCurrentThread());
  }

  EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());
}

TEST(SequenceTokenTest, ToInternalValue) {
  const SequenceToken token1 = SequenceToken::Create();
  const SequenceToken token2 = SequenceToken::Create();

  // Confirm that internal values are unique.
  EXPECT_NE(token1.ToInternalValue(), token2.ToInternalValue());
}

// Expect a default-constructed TaskToken to be invalid and not equal to
// another invalid TaskToken.
TEST(TaskTokenTest, InvalidDefaultConstructed) {
  EXPECT_FALSE(TaskToken().IsValid());
  EXPECT_NE(TaskToken(), TaskToken());
}

// Expect a TaskToken returned by TaskToken::GetForCurrentThread() outside the
// scope of a ScopedSetSequenceTokenForCurrentThread to be invalid.
TEST(TaskTokenTest, InvalidOutsideScope) {
  EXPECT_FALSE(TaskToken::GetForCurrentThread().IsValid());
}

// Expect an invalid TaskToken not to be equal with a valid TaskToken.
TEST(TaskTokenTest, ValidNotEqualsInvalid) {
  ScopedSetSequenceTokenForCurrentThread
      scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
  TaskToken valid = TaskToken::GetForCurrentThread();
  TaskToken invalid;
  EXPECT_NE(valid, invalid);
}

// Expect TaskTokens returned by TaskToken::GetForCurrentThread() in the scope
// of the same ScopedSetSequenceTokenForCurrentThread instance to be
// valid and equal with each other.
TEST(TaskTokenTest, EqualInSameScope) {
  ScopedSetSequenceTokenForCurrentThread
      scoped_set_sequence_token_for_current_thread(SequenceToken::Create());

  const TaskToken token_a = TaskToken::GetForCurrentThread();
  const TaskToken token_b = TaskToken::GetForCurrentThread();

  EXPECT_TRUE(token_a.IsValid());
  EXPECT_TRUE(token_b.IsValid());
  EXPECT_EQ(token_a, token_b);
}

// Expect TaskTokens returned by TaskToken::GetForCurrentThread() in the scope
// of different ScopedSetSequenceTokenForCurrentThread instances to be
// valid but not equal to each other.
TEST(TaskTokenTest, NotEqualInDifferentScopes) {
  TaskToken token_a;
  TaskToken token_b;

  {
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
    token_a = TaskToken::GetForCurrentThread();
  }
  {
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
    token_b = TaskToken::GetForCurrentThread();
  }

  EXPECT_TRUE(token_a.IsValid());
  EXPECT_TRUE(token_b.IsValid());
  EXPECT_NE(token_a, token_b);
}

}  // namespace base
