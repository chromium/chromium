// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_transcript_matcher.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"

namespace ttc {

namespace {

std::vector<std::u16string> TokenizeCaseFolded(std::u16string_view input) {
  const std::u16string folded = base::i18n::FoldCase(input);
  std::vector<std::u16string> tokens;
  base::i18n::BreakIterator iter(folded, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return tokens;
  }
  while (iter.Advance()) {
    if (iter.IsWord()) {
      tokens.emplace_back(iter.GetString());
    }
  }
  return tokens;
}

}  // namespace

HotlMatchResult MatchHotlTranscript(std::u16string_view transcript,
                                    std::u16string_view localized_yes,
                                    std::u16string_view localized_no) {
  const std::vector<std::u16string> transcript_tokens =
      TokenizeCaseFolded(transcript);
  const std::vector<std::u16string> yes_tokens =
      TokenizeCaseFolded(localized_yes);
  const std::vector<std::u16string> no_tokens =
      TokenizeCaseFolded(localized_no);
  // A bad label must never let a transcript approve the action.
  if (transcript_tokens.empty() || yes_tokens.empty() || no_tokens.empty() ||
      yes_tokens == no_tokens) {
    return HotlMatchResult::kAmbiguous;
  }

  const bool has_yes =
      !std::ranges::search(transcript_tokens, yes_tokens).empty();
  const bool has_no =
      !std::ranges::search(transcript_tokens, no_tokens).empty();
  if (has_yes && !has_no) {
    return HotlMatchResult::kYes;
  }
  if (has_no && !has_yes) {
    return HotlMatchResult::kNo;
  }
  return HotlMatchResult::kAmbiguous;
}

}  // namespace ttc
