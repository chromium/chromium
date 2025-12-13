// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_signal_utils.h"

#include <algorithm>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace contextual_tasks {

namespace {

// TODO: crbug.com/452036470 - Do i18n of this set and related logic.
constexpr auto kStopWords = base::MakeFixedFlatSet<std::string>(
    {"a",      "an",    "the",  "and",  "but",   "or",   "is",   "are",
     "was",    "were",  "be",   "been", "being", "in",   "on",   "at",
     "of",     "for",   "with", "it",   "its",   "if",   "that", "this",
     "those",  "these", "i",    "me",   "my",    "he",   "she",  "we",
     "you",    "from",  "by",   "not",  "no",    "will", "wasn", "weren",
     "hasn",   "haven", "hadn", "don",  "doesn", "didn", "can",  "could",
     "should", "would", "s",    "t"});

std::string GetNormalizedString(const std::string& str) {
  std::string normalized_str = base::ToLowerASCII(str);
  std::replace_if(
      normalized_str.begin(), normalized_str.end(),
      [](unsigned char c) { return std::ispunct(c); }, ' ');
  return normalized_str;
}

base::flat_set<std::string> NormalizeAndTokenize(const std::string& str) {
  std::vector<std::string> normalized_words_vec =
      base::SplitString(GetNormalizedString(str), " ", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  base::flat_set<std::string> normalized_words(normalized_words_vec.begin(),
                                               normalized_words_vec.end());
  return normalized_words;
}

}  // namespace

int GetMatchingWordsCount(const std::string& query,
                          const std::string& candidate) {
  base::flat_set<std::string> query_words = NormalizeAndTokenize(query);
  base::flat_set<std::string> candidate_words = NormalizeAndTokenize(candidate);

  int num_matching_words = 0;
  for (const std::string& word : query_words) {
    if (!kStopWords.contains(word) && candidate_words.contains(word)) {
      num_matching_words++;
    }
  }

  return num_matching_words;
}

}  // namespace contextual_tasks
