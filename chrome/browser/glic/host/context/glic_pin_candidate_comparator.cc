// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pin_candidate_comparator.h"

#include <tuple>

#include "base/i18n/char_iterator.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {

bool ContainsWhitespace(std::u16string_view text) {
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    if (base::IsUnicodeWhitespace(iter.get())) {
      return true;
    }
  }
  return false;
}

SearchResult Search(std::u16string_view title, const std::u16string& query) {
  SearchResult result;
  size_t index;
  size_t length;
  if (base::i18n::StringSearchIgnoringCaseAndAccents(query, title, &index,
                                                     &length)) {
    result.matches_prefix = index == 0u;
    result.contained = true;
    if (ContainsWhitespace(query)) {
      // Cannot match a word prefix if `query` contains whitespace.
      return result;
    }
    std::vector<std::u16string_view> words =
        base::SplitStringPiece(title, base::kWhitespaceUTF16,
                               base::WhitespaceHandling::TRIM_WHITESPACE,
                               base::SplitResult::SPLIT_WANT_NONEMPTY);
    for (auto& word : words) {
      if (base::i18n::StringSearchIgnoringCaseAndAccents(query, word, &index,
                                                         &length)) {
        if (index == 0u) {
          result.matches_word_prefix = true;
          break;
        }
      }
    }
  }
  return result;
}

}  // namespace

GlicPinCandidateComparator::GlicPinCandidateComparator(
    const base::optional_ref<const std::string> query) {
  if (query) {
    query_ = base::UTF8ToUTF16(*query);
  }
}

GlicPinCandidateComparator::~GlicPinCandidateComparator() = default;

bool GlicPinCandidateComparator::operator()(content::WebContents* a,
                                            content::WebContents* b) {
  if (query_.empty()) {
    return a->GetLastActiveTimeTicks() > b->GetLastActiveTimeTicks();
  }

  const SearchResult a_results = GetSearchResults(a->GetTitle());
  const SearchResult b_results = GetSearchResults(b->GetTitle());

  // Use last active time as a tie-breaker when other search result criteria are
  // equal.
  auto a_tie_breaker = a->GetLastActiveTimeTicks();
  auto b_tie_breaker = b->GetLastActiveTimeTicks();

  // Compare WebContents based on a tuple of search result criteria and
  // last active time. The comparison order prioritizes:
  // 1. `matches_prefix`: Whether the query matches the beginning of the title.
  // 2. `matches_word_prefix`: Whether the query matches the beginning of a word
  //    in the title.
  // 3. `contained`: Whether the query is contained anywhere in the title.
  // 4. `last_active_time`: Used as a tie-breaker, more recent first.
  return std::tie(a_results.matches_prefix, a_results.matches_word_prefix,
                  a_results.contained, a_tie_breaker) >
         std::tie(b_results.matches_prefix, b_results.matches_word_prefix,
                  b_results.contained, b_tie_breaker);
}

const SearchResult GlicPinCandidateComparator::GetSearchResults(
    std::u16string_view title) {
  std::u16string title_str(title);
  auto it = search_results_cache_.find(title_str);
  if (it == search_results_cache_.end()) {
    it = search_results_cache_
             .insert({std::move(title_str), Search(title, query_)})
             .first;
  }
  return it->second;
}

}  // namespace glic
