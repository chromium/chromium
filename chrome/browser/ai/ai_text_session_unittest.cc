// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;

const uint32_t kTestMaxContextToken = 10u;
const uint32_t kTestSystemPromptToken = 5u;

// Tests the `AITextSession::Context` that's initialized with/without any system
// prompt.
class AITextSessionTest
    : public testing::Test,
      public testing::WithParamInterface</*is_init_with_system_prompt=*/bool> {
 public:
  bool IsInitializedWithSystemPrompt() { return GetParam(); }

  uint32_t GetMaxContextToken() {
    return IsInitializedWithSystemPrompt()
               ? kTestMaxContextToken - kTestSystemPromptToken
               : kTestMaxContextToken;
  }

  std::string GetSystemPromptPrefix() {
    return IsInitializedWithSystemPrompt() ? "system prompt\n" : "";
  }

  AITextSession::Context context_{
      kTestMaxContextToken,
      IsInitializedWithSystemPrompt()
          ? std::optional<
                AITextSession::Context::ContextItem>{{"system prompt",
                                                      kTestSystemPromptToken}}
          : std::nullopt};
};

INSTANTIATE_TEST_SUITE_P(All,
                         AITextSessionTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSystemPrompt"
                                             : "WithoutSystemPrompt";
                         });

// Tests `GetContextString()` and `HasContextItem()` when the context is empty.
TEST_P(AITextSessionTest, TestContextOperation_Empty) {
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix());

  if (IsInitializedWithSystemPrompt()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}

// Tests `GetContextString()` and `HasContextItem()` when some items are added
// to the context.
TEST_P(AITextSessionTest, TestContextOperation_NonEmpty) {
  context_.AddContextItem({"test", 1u});
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix() + "test");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem({" test again", 2u});
  EXPECT_EQ(context_.GetContextString(),
            GetSystemPromptPrefix() + "test test again");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_P(AITextSessionTest, TestContextOperation_Overflow) {
  context_.AddContextItem({"test", 1u});
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix() + "test");
  EXPECT_TRUE(context_.HasContextItem());

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  context_.AddContextItem({"test long token", GetMaxContextToken()});
  EXPECT_EQ(context_.GetContextString(),
            GetSystemPromptPrefix() + "test long token");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow on
// the first insertion.
TEST_P(AITextSessionTest, TestContextOperation_OverflowOnFirstItem) {
  context_.AddContextItem({"test very long token", GetMaxContextToken() + 1u});
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix());
  if (IsInitializedWithSystemPrompt()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}
