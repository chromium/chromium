// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_transcript_matcher.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

TEST(HotlTranscriptMatcherTest, MatchesYesAndNoTokens) {
  EXPECT_EQ(MatchHotlTranscript(u"Yes", u"Yes", u"No"), HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscript(u"yes, please click it!", u"Yes", u"No"),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscript(u"yes yes", u"Yes", u"No"),
            HotlMatchResult::kYes);

  EXPECT_EQ(MatchHotlTranscript(u"No", u"Yes", u"No"), HotlMatchResult::kNo);
  EXPECT_EQ(MatchHotlTranscript(u"Wait... no!", u"Yes", u"No"),
            HotlMatchResult::kNo);
}

TEST(HotlTranscriptMatcherTest, RejectsSubstringsAndAmbiguousInput) {
  EXPECT_EQ(MatchHotlTranscript(u"yesterday", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscript(u"not now", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscript(u"", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscript(u"...?!", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscript(u"Yes no wait", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
}

// A bad label must never let a transcript approve the action.
TEST(HotlTranscriptMatcherTest, InvalidLabelsAreAmbiguous) {
  EXPECT_EQ(MatchHotlTranscript(u"yes", u"", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscript(u"no", u"Yes", u"  "),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscript(u"yes", u"Yes", u"YES"),
            HotlMatchResult::kAmbiguous);
}

TEST(HotlTranscriptMatcherTest, MatchesMultiWordLabelsInOrder) {
  EXPECT_EQ(MatchHotlTranscript(u"ok, go ahead now", u"Go ahead", u"Stop"),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscript(u"go now, ahead", u"Go ahead", u"Stop"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscript(u"ahead go", u"Go ahead", u"Stop"),
            HotlMatchResult::kAmbiguous);
}

TEST(HotlTranscriptMatcherTest, MatchesNonAsciiLabelsCaseInsensitively) {
  EXPECT_EQ(MatchHotlTranscript(u"S\u00cd, por favor", u"S\u00ed", u"No"),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscript(u"NEIN!", u"Ja", u"Nein"),
            HotlMatchResult::kNo);
  // Case folding, unlike lowercasing, maps sharp s and "SS" to the same text.
  EXPECT_EQ(MatchHotlTranscript(u"GROSS", u"Gro\u00df", u"Klein"),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscript(
                u"\u306f\u3044\u3001\u304a\u9858\u3044\u3057\u307e\u3059",
                u"\u306f\u3044", u"\u3044\u3044\u3048"),
            HotlMatchResult::kYes);
  EXPECT_EQ(
      MatchHotlTranscript(u"\u3044\u3044\u3048\u3001\u9055\u3044\u307e\u3059",
                          u"\u306f\u3044", u"\u3044\u3044\u3048"),
      HotlMatchResult::kNo);
#if !BUILDFLAG(IS_ANDROID)
  // Android builds omit the CJK BreakIterator dictionary.
  EXPECT_EQ(
      MatchHotlTranscript(u"\u306f\u3044\u304a\u9858\u3044\u3057\u307e\u3059",
                          u"\u306f\u3044", u"\u3044\u3044\u3048"),
      HotlMatchResult::kYes);
#endif
}

}  // namespace
}  // namespace ttc
