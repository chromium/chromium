// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/text_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

TEST(TextUtilsTest, FindsLastSentenceEnd) {
  // Simple test cases.
  EXPECT_EQ(FindLastSentenceEnd(u"This is a test.", 15u), 14u);
  EXPECT_EQ(FindLastSentenceEnd(u"This is a test.", 14u), kUndefined);

  // Ignores non-terminal dot.
  EXPECT_EQ(FindLastSentenceEnd(u"Hi! There are 1.5 million people.", 20u), 2u);
  EXPECT_EQ(FindLastSentenceEnd(u"What? Mr. Smith isn't here!", 20u), 4u);
  EXPECT_EQ(FindLastSentenceEnd(u"What? (Mr. Smith isn't here!)", 20u), 4u);

  // Ends with section end.
  EXPECT_EQ(FindLastSentenceEnd(u"He said: \"You are superb!\"", 26u), 25u);
  EXPECT_EQ(FindLastSentenceEnd(u"(This is a test.) Hi!", 19u), 16u);

  // Ends with emoticon.
  EXPECT_EQ(FindLastSentenceEnd(u"Thank you :) Have a nice day!", 20u), 11u);
  EXPECT_EQ(FindLastSentenceEnd(u"I am so sorry :-(", 17u), 16u);

  // Multiple lines cases.
  EXPECT_EQ(FindLastSentenceEnd(u"This is a dog\n\nThat is a cat", 20u), 13u);
  EXPECT_EQ(FindLastSentenceEnd(u"This is a dog\n", 13u), 12u);
}

TEST(TextUtilsTest, FindsNextSentenceEnd) {
  // Simple test cases.
  EXPECT_EQ(FindNextSentenceEnd(u"This is a test.", 15u), kUndefined);
  EXPECT_EQ(FindNextSentenceEnd(u"This is a test.", 14u), 14u);
  EXPECT_EQ(FindNextSentenceEnd(u"This is a test.", 0u), 14u);

  // Ignores non-terminal dot.
  EXPECT_EQ(FindNextSentenceEnd(u"Hi! There are 1.5 million people.", 8u), 32u);
  EXPECT_EQ(FindNextSentenceEnd(u"What? Mr. Smith isn't here!", 6u), 26u);
  EXPECT_EQ(FindNextSentenceEnd(u"What? (Mr. Smith isn't here!)", 6u), 28u);

  // Ends with section end.
  EXPECT_EQ(FindNextSentenceEnd(u"He said: \"You are superb!\"", 0u), 25u);
  EXPECT_EQ(FindNextSentenceEnd(u"(This is a test.) Hi!", 10u), 16u);

  // Ends with emoticon.
  EXPECT_EQ(FindNextSentenceEnd(u"Thank you :) Have a nice day!", 0u), 11u);
  EXPECT_EQ(FindNextSentenceEnd(u"I am so sorry :-(", 16u), 16u);

  // Multiple lines cases.
  EXPECT_EQ(FindNextSentenceEnd(u"This is a dog\n\nThat is a cat", 0u), 12u);
}

TEST(TextUtilsTest, FindLastSentence) {
  EXPECT_EQ(FindLastSentence(u"This is a test.", 14u), Sentence());
  EXPECT_EQ(FindLastSentence(u"This is a test.", 15u), Sentence());
  EXPECT_EQ(FindLastSentence(u"This is a test. ", 15u),
            Sentence(gfx::Range(0, 15), u"This is a test."));
  EXPECT_EQ(FindLastSentence(u"Hi! This is a test.  ", 19u),
            Sentence(gfx::Range(4, 19), u"This is a test."));

  EXPECT_EQ(FindLastSentence(u"Hi! There are 1.5 million people.", 20u),
            Sentence(gfx::Range(0, 3), u"Hi!"));
  EXPECT_EQ(FindLastSentence(u"What? Mr. Smith isn't here!", 20u),
            Sentence(gfx::Range(0, 5), u"What?"));
  EXPECT_EQ(FindLastSentence(u"What? (Mr. Smith isn't here!)", 20u),
            Sentence(gfx::Range(0, 5), u"What?"));

  EXPECT_EQ(FindLastSentence(u"Thank you :) Have a nice day!", 20u),
            Sentence(gfx::Range(0, 12), u"Thank you :)"));

  EXPECT_EQ(FindLastSentence(u"This is a dog\nThat is a cat", 20u),
            Sentence(gfx::Range(0, 13), u"This is a dog"));
  EXPECT_EQ(FindLastSentence(u"This is a dog\n\nThat is a cat", 20u),
            Sentence());
  EXPECT_EQ(FindLastSentence(u"This is a dog\n", 13u), Sentence());
}

TEST(TextUtilsTest, FindCurrentSentence) {
  EXPECT_EQ(FindCurrentSentence(u"This is a test.", 14u),
            Sentence(gfx::Range(0, 15), u"This is a test."));
  EXPECT_EQ(FindCurrentSentence(u"This is a test.", 15u),
            Sentence(gfx::Range(0, 15), u"This is a test."));
  EXPECT_EQ(FindCurrentSentence(u"This is a test. ", 15u), Sentence());
  EXPECT_EQ(FindCurrentSentence(u"Hi! This is a test.  ", 18u),
            Sentence(gfx::Range(4, 19), u"This is a test."));

  EXPECT_EQ(FindCurrentSentence(u"Hi! There are 1.5 million people.", 20u),
            Sentence(gfx::Range(4, 33), u"There are 1.5 million people."));
  EXPECT_EQ(FindCurrentSentence(u"What? Mr. Smith isn't here!", 20u),
            Sentence(gfx::Range(6, 27), u"Mr. Smith isn't here!"));
  EXPECT_EQ(FindCurrentSentence(u"What? (Mr. Smith isn't here!)", 20u),
            Sentence(gfx::Range(6, 29), u"(Mr. Smith isn't here!)"));

  EXPECT_EQ(FindCurrentSentence(u"Thank you :) Have a nice day!", 20u),
            Sentence(gfx::Range(13, 29), u"Have a nice day!"));

  EXPECT_EQ(FindCurrentSentence(u"This is a dog\nThat is a cat", 20u),
            Sentence(gfx::Range(14, 27), u"That is a cat"));
  EXPECT_EQ(FindCurrentSentence(u"This is a dog\n", 13u),
            Sentence(gfx::Range(0, 13), u"This is a dog"));
}

}  // namespace
}  // namespace input_method
}  // namespace ash
