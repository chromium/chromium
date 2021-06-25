// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/text_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace text_utils {
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

}  // namespace
}  // namespace text_utils
}  // namespace chromeos
