// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/text_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

TEST(TextUtilsTest, FindsLastSentenceEnd) {
  // Simple test cases.
  EXPECT_EQ(FindLastSentenceEnd(u"This is a test.", 15), 14);
  EXPECT_EQ(FindLastSentenceEnd(u"This is a test.", 14), kUndefined);

  // Ignores non-terminal dot.
  EXPECT_EQ(FindLastSentenceEnd(u"Hi! There are 1.5 million people.", 20), 2);
  EXPECT_EQ(FindLastSentenceEnd(u"What? Mr. Smith isn't here!", 20), 4);
  EXPECT_EQ(FindLastSentenceEnd(u"What? (Mr. Smith isn't here!)", 20), 4);

  // Ends with section end.
  EXPECT_EQ(FindLastSentenceEnd(u"He said: \"You are superb!\"", 26), 25);
  EXPECT_EQ(FindLastSentenceEnd(u"(This is a test.) Hi!", 19), 16);

  // Ends with emoticon.
  EXPECT_EQ(FindLastSentenceEnd(u"Thank you :) Have a nice day!", 20), 11);
  EXPECT_EQ(FindLastSentenceEnd(u"I am so sorry :-(", 17), 16);

  // Multiple lines cases.
  EXPECT_EQ(FindLastSentenceEnd(u"This is a dog\n\nThat is a cat", 20), 13);
  EXPECT_EQ(FindLastSentenceEnd(u"This is a dog\n", 13), 12);
}

TEST(TextUtilsTest, FindsNextSentenceEnd) {
  // Simple test cases.
  EXPECT_EQ(FindNextSentenceEnd(u"This is a test.", 15), kUndefined);
  EXPECT_EQ(FindNextSentenceEnd(u"This is a test.", 14), 14);
  EXPECT_EQ(FindNextSentenceEnd(u"This is a test.", 0), 14);

  // Ignores non-terminal dot.
  EXPECT_EQ(FindNextSentenceEnd(u"Hi! There are 1.5 million people.", 8), 32);
  EXPECT_EQ(FindNextSentenceEnd(u"What? Mr. Smith isn't here!", 6), 26);
  EXPECT_EQ(FindNextSentenceEnd(u"What? (Mr. Smith isn't here!)", 6), 28);

  // Ends with section end.
  EXPECT_EQ(FindNextSentenceEnd(u"He said: \"You are superb!\"", 0), 25);
  EXPECT_EQ(FindNextSentenceEnd(u"(This is a test.) Hi!", 10), 16);

  // Ends with emoticon.
  EXPECT_EQ(FindNextSentenceEnd(u"Thank you :) Have a nice day!", 0), 11);
  EXPECT_EQ(FindNextSentenceEnd(u"I am so sorry :-(", 16), 16);

  // Multiple lines cases.
  EXPECT_EQ(FindNextSentenceEnd(u"This is a dog\n\nThat is a cat", 0), 12);
}

TEST(TextUtilsTest, FindLastSentence) {
  EXPECT_EQ(FindLastSentence(u"This is a test.", 14), Sentence());
  EXPECT_EQ(FindLastSentence(u"This is a test.", 15), Sentence());
  EXPECT_EQ(FindLastSentence(u"This is a test. ", 15),
            Sentence(gfx::Range(0, 15), u"This is a test."));
  EXPECT_EQ(FindLastSentence(u"Hi! This is a test.  ", 19),
            Sentence(gfx::Range(4, 19), u"This is a test."));

  EXPECT_EQ(FindLastSentence(u"Hi! There are 1.5 million people.", 20),
            Sentence(gfx::Range(0, 3), u"Hi!"));
  EXPECT_EQ(FindLastSentence(u"What? Mr. Smith isn't here!", 20),
            Sentence(gfx::Range(0, 5), u"What?"));
  EXPECT_EQ(FindLastSentence(u"What? (Mr. Smith isn't here!)", 20),
            Sentence(gfx::Range(0, 5), u"What?"));

  EXPECT_EQ(FindLastSentence(u"Thank you :) Have a nice day!", 20),
            Sentence(gfx::Range(0, 12), u"Thank you :)"));

  EXPECT_EQ(FindLastSentence(u"This is a dog\nThat is a cat", 20),
            Sentence(gfx::Range(0, 13), u"This is a dog"));
  EXPECT_EQ(FindLastSentence(u"This is a dog\n\nThat is a cat", 20),
            Sentence());
  EXPECT_EQ(FindLastSentence(u"This is a dog\n", 13), Sentence());
}

TEST(TextUtilsTest, FindCurrentSentence) {
  EXPECT_EQ(FindCurrentSentence(u"This is a test.", 14),
            Sentence(gfx::Range(0, 15), u"This is a test."));
  EXPECT_EQ(FindCurrentSentence(u"This is a test.", 15),
            Sentence(gfx::Range(0, 15), u"This is a test."));
  EXPECT_EQ(FindCurrentSentence(u"This is a test. ", 15), Sentence());
  EXPECT_EQ(FindCurrentSentence(u"Hi! This is a test.  ", 18),
            Sentence(gfx::Range(4, 19), u"This is a test."));

  EXPECT_EQ(FindCurrentSentence(u"Hi! There are 1.5 million people.", 20),
            Sentence(gfx::Range(4, 33), u"There are 1.5 million people."));
  EXPECT_EQ(FindCurrentSentence(u"What? Mr. Smith isn't here!", 20),
            Sentence(gfx::Range(6, 27), u"Mr. Smith isn't here!"));
  EXPECT_EQ(FindCurrentSentence(u"What? (Mr. Smith isn't here!)", 20),
            Sentence(gfx::Range(6, 29), u"(Mr. Smith isn't here!)"));

  EXPECT_EQ(FindCurrentSentence(u"Thank you :) Have a nice day!", 20),
            Sentence(gfx::Range(13, 29), u"Have a nice day!"));

  EXPECT_EQ(FindCurrentSentence(u"This is a dog\nThat is a cat", 20),
            Sentence(gfx::Range(14, 27), u"That is a cat"));
  EXPECT_EQ(FindCurrentSentence(u"This is a dog\n", 13),
            Sentence(gfx::Range(0, 13), u"This is a dog"));
}

}  // namespace
}  // namespace input_method
}  // namespace ash
