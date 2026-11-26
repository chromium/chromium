// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_transcript_matcher.h"

#include <string_view>

#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ttc {
namespace {

TEST(HotlTranscriptMatcherTest, MatchesLocalizedUiResources) {
  EXPECT_EQ(
      MatchHotlTranscript(l10n_util::GetStringUTF16(IDS_TTC_CONFIRMATION_YES)),
      HotlMatchResult::kYes);
  EXPECT_EQ(
      MatchHotlTranscript(l10n_util::GetStringUTF16(IDS_TTC_CONFIRMATION_NO)),
      HotlMatchResult::kNo);
  EXPECT_EQ(MatchHotlTranscript(u"zebra 4921"), HotlMatchResult::kAmbiguous);
}

TEST(HotlTranscriptMatcherTest, MatchesYesAndNoTokens) {
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"Yes", u"Yes", u"No"),
            HotlMatchResult::kYes);
  EXPECT_EQ(
      MatchHotlTranscriptForTesting(u"yes, please click it!", u"Yes", u"No"),
      HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"yes yes", u"Yes", u"No"),
            HotlMatchResult::kYes);

  EXPECT_EQ(MatchHotlTranscriptForTesting(u"No", u"Yes", u"No"),
            HotlMatchResult::kNo);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"Wait... no!", u"Yes", u"No"),
            HotlMatchResult::kNo);
}

TEST(HotlTranscriptMatcherTest, RejectsSubstringsAndAmbiguousInput) {
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"yesterday", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"not now", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"...?!", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"Yes no wait", u"Yes", u"No"),
            HotlMatchResult::kAmbiguous);
}

// A bad label must never let a transcript approve the action.
TEST(HotlTranscriptMatcherTest, InvalidLabelsAreAmbiguous) {
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"yes", u"", u"No"),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"no", u"Yes", u"  "),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"yes", u"Yes", u"YES"),
            HotlMatchResult::kAmbiguous);
}

TEST(HotlTranscriptMatcherTest, MatchesMultiWordLabelsInOrder) {
  EXPECT_EQ(
      MatchHotlTranscriptForTesting(u"ok, go ahead now", u"Go ahead", u"Stop"),
      HotlMatchResult::kYes);
  EXPECT_EQ(
      MatchHotlTranscriptForTesting(u"go now, ahead", u"Go ahead", u"Stop"),
      HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"ahead go", u"Go ahead", u"Stop"),
            HotlMatchResult::kAmbiguous);
}

TEST(HotlTranscriptMatcherTest, MatchesNonAsciiLabelsCaseInsensitively) {
  EXPECT_EQ(
      MatchHotlTranscriptForTesting(u"S\u00cd, por favor", u"S\u00ed", u"No"),
      HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"NEIN!", u"Ja", u"Nein"),
            HotlMatchResult::kNo);
  // Case folding, unlike lowercasing, maps sharp s and "SS" to the same text.
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"GROSS", u"Gro\u00df", u"Klein"),
            HotlMatchResult::kYes);
}

TEST(HotlTranscriptMatcherTest, MatchesSpanishTokens) {
  constexpr std::u16string_view kYes = u"Sí";
  constexpr std::u16string_view kNo = u"No";

  EXPECT_EQ(MatchHotlTranscriptForTesting(u"¡Sí, haz clic!", kYes, kNo),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"sí por favor", kYes, kNo),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"¡No, espera!", kYes, kNo),
            HotlMatchResult::kNo);

  // Substrings and unaccented "si" ("if") must not approve the action.
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"sigue adelante", kYes, kNo),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"esta noche", kYes, kNo),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"si quieres", kYes, kNo),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"Sí, espera, no", kYes, kNo),
            HotlMatchResult::kAmbiguous);
}

TEST(HotlTranscriptMatcherTest, MatchesCjkTokens) {
  constexpr std::u16string_view kYes = u"はい";
  constexpr std::u16string_view kNo = u"いいえ";

  // Punctuated Japanese phrases segment identically on all platforms.
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"はい、お願いします", kYes, kNo),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"いいえ、違います", kYes, kNo),
            HotlMatchResult::kNo);

#if !BUILDFLAG(IS_ANDROID)
  // Android builds omit the CJK BreakIterator dictionary, so unspaced phrases
  // require non-Android dictionary segmentation.
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"はいお願いします", kYes, kNo),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"はいどうぞ", kYes, kNo),
            HotlMatchResult::kYes);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"いいえ違います", kYes, kNo),
            HotlMatchResult::kNo);

  // "はいる" ("to enter") does not segment into "はい" + "る", and
  // contradictory YES+NO phrases fail closed.
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"はいる", kYes, kNo),
            HotlMatchResult::kAmbiguous);
  EXPECT_EQ(MatchHotlTranscriptForTesting(u"はいやっぱりいいえ", kYes, kNo),
            HotlMatchResult::kAmbiguous);
#endif
}

}  // namespace
}  // namespace ttc
