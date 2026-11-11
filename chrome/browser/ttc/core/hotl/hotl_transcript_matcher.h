// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_HOTL_TRANSCRIPT_MATCHER_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_HOTL_TRANSCRIPT_MATCHER_H_

#include <string_view>

namespace ttc {

// Result of matching a finalized SODA speech transcript against the localized
// YES and NO confirmation keywords.
enum class HotlMatchResult {
  // Transcript contains the localized YES token sequence and does not contain
  // the localized NO token sequence.
  kYes,
  // Transcript contains the localized NO token sequence and does not contain
  // the localized YES token sequence.
  kNo,
  // Transcript contains neither keyword, or contains both YES and NO keywords.
  kAmbiguous,
};

// Matches a finalized user speech transcript against the localized
// confirmation labels. Matching is case-insensitive and works on whole words,
// so "yesterday" does not match "yes".
HotlMatchResult MatchHotlTranscript(std::u16string_view transcript,
                                    std::u16string_view localized_yes,
                                    std::u16string_view localized_no);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_HOTL_TRANSCRIPT_MATCHER_H_
