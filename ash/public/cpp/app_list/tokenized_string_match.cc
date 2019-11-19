// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/tokenized_string_match.h"

#include <stddef.h>

#include <cmath>

#include "ash/public/cpp/app_list/tokenized_string_char_iterator.h"
#include "base/i18n/string_search.h"
#include "base/logging.h"
#include "base/macros.h"

namespace ash {

namespace {

// The factors below are applied when the current char of query matches
// the current char of the text to be matched. Different factors are chosen
// based on where the match happens. kIsPrefixMultiplier is used when the
// matched portion is a prefix of both the query and the text, which implies
// that the matched chars are at the same position in query and text. This is
// the most preferred case thus it has the highest score. When the current char
// of the query and the text does not match, the algorithm moves to the next
// token in the text and try to match from there. kIsFrontOfWordMultipler will
// be used if the first char of the token matches the current char of the query.
// Otherwise, the match is considered as weak and kIsWeakHitMultiplier is
// used.
// Examples:
//   Suppose the text to be matched is 'Google Chrome'.
//   Query 'go' would yield kIsPrefixMultiplier for each char.
//   Query 'gc' would use kIsPrefixMultiplier for 'g' and
//       kIsFrontOfWordMultipler for 'c'.
//   Query 'ch' would use kIsFrontOfWordMultipler for 'c' and
//       kIsWeakHitMultiplier for 'h'.
//   Query 'oo' does not match any prefix and would use the substring match
//       fallback, thus kIsSubstringMultiplier is used for each char.
const double kIsPrefixMultiplier = 1.0;
const double kIsFrontOfWordMultipler = 0.8;
const double kIsWeakHitMultiplier = 0.6;
const double kIsSubstringMultiplier = 0.4;

// A relevance score that represents no match.
const double kNoMatchScore = 0.0;

// PrefixMatcher matches the chars of a given query as prefix of tokens in
// a given text or as prefix of the acronyms of those text tokens.
class PrefixMatcher {
 public:
  PrefixMatcher(const TokenizedString& query, const TokenizedString& text)
      : query_iter_(query),
        text_iter_(text),
        current_match_(gfx::Range::InvalidRange()),
        current_relevance_(kNoMatchScore) {}

  // Invokes RunMatch to perform prefix match. Use |states_| as a stack to
  // perform DFS (depth first search) so that all possible matches are
  // attempted. Stops on the first full match and returns true. Otherwise,
  // returns false to indicate no match.
  bool Match() {
    while (!RunMatch()) {
      // No match found and no more states to try. Bail out.
      if (states_.empty()) {
        current_relevance_ = kNoMatchScore;
        current_hits_.clear();
        return false;
      }

      PopState();

      // Skip restored match to try other possibilites.
      AdvanceToNextTextToken();
    }

    if (current_match_.IsValid())
      current_hits_.push_back(current_match_);

    return true;
  }

  double relevance() const { return current_relevance_; }
  const TokenizedStringMatch::Hits& hits() const { return current_hits_; }

 private:
  // Context record of a match.
  struct State {
    State() : relevance(kNoMatchScore) {}
    State(double relevance,
          const gfx::Range& current_match,
          const TokenizedStringMatch::Hits& hits,
          const TokenizedStringCharIterator& query_iter,
          const TokenizedStringCharIterator& text_iter)
        : relevance(relevance),
          current_match(current_match),
          hits(hits.begin(), hits.end()),
          query_iter_state(query_iter.GetState()),
          text_iter_state(text_iter.GetState()) {}

    // The current score of the processed query chars.
    double relevance;

    // Current matching range.
    gfx::Range current_match;

    // Completed matching ranges of the processed query chars.
    TokenizedStringMatch::Hits hits;

    // States of the processed query and text chars.
    TokenizedStringCharIterator::State query_iter_state;
    TokenizedStringCharIterator::State text_iter_state;
  };
  typedef std::vector<State> States;

  // Match chars from the query and text one by one. For each matching char,
  // calculate relevance and matching ranges. And the current stats is
  // recorded so that the match could be skipped later to try other
  // possiblities. Repeat until any of the iterators run out. Return true if
  // query iterator runs out, i.e. all chars in query are matched.
  bool RunMatch() {
    while (!query_iter_.end() && !text_iter_.end()) {
      if (query_iter_.Get() == text_iter_.Get()) {
        PushState();

        if (query_iter_.GetArrayPos() == text_iter_.GetArrayPos())
          current_relevance_ += kIsPrefixMultiplier;
        else if (text_iter_.IsFirstCharOfToken())
          current_relevance_ += kIsFrontOfWordMultipler;
        else
          current_relevance_ += kIsWeakHitMultiplier;

        if (!current_match_.IsValid())
          current_match_.set_start(text_iter_.GetArrayPos());
        current_match_.set_end(text_iter_.GetArrayPos() +
                               text_iter_.GetCharSize());

        query_iter_.NextChar();
        text_iter_.NextChar();
      } else {
        AdvanceToNextTextToken();
      }
    }

    return query_iter_.end();
  }

  // Skip to the next text token and close current match. Invoked when a
  // mismatch happens or to skip a restored match.
  void AdvanceToNextTextToken() {
    if (current_match_.IsValid()) {
      current_hits_.push_back(current_match_);
      current_match_ = gfx::Range::InvalidRange();
    }

    text_iter_.NextToken();
  }

  void PushState() {
    states_.push_back(State(current_relevance_, current_match_, current_hits_,
                            query_iter_, text_iter_));
  }

  void PopState() {
    DCHECK(!states_.empty());

    State& last_match = states_.back();
    current_relevance_ = last_match.relevance;
    current_match_ = last_match.current_match;
    current_hits_.swap(last_match.hits);
    query_iter_.SetState(last_match.query_iter_state);
    text_iter_.SetState(last_match.text_iter_state);

    states_.pop_back();
  }

  TokenizedStringCharIterator query_iter_;
  TokenizedStringCharIterator text_iter_;

  States states_;
  gfx::Range current_match_;

  double current_relevance_;
  TokenizedStringMatch::Hits current_hits_;

  DISALLOW_COPY_AND_ASSIGN(PrefixMatcher);
};

}  // namespace

TokenizedStringMatch::TokenizedStringMatch() : relevance_(kNoMatchScore) {}

TokenizedStringMatch::~TokenizedStringMatch() = default;

bool TokenizedStringMatch::Calculate(const TokenizedString& query,
                                     const TokenizedString& text) {
  relevance_ = kNoMatchScore;
  hits_.clear();

  PrefixMatcher matcher(query, text);
  if (matcher.Match()) {
    relevance_ = matcher.relevance();
    hits_.assign(matcher.hits().begin(), matcher.hits().end());
  }

  // Substring match as a fallback.
  if (relevance_ == kNoMatchScore) {
    size_t substr_match_start = 0;
    size_t substr_match_length = 0;
    if (base::i18n::StringSearchIgnoringCaseAndAccents(
            query.text(), text.text(), &substr_match_start,
            &substr_match_length)) {
      relevance_ = kIsSubstringMultiplier * substr_match_length;
      hits_.push_back(gfx::Range(substr_match_start,
                                 substr_match_start + substr_match_length));
    }
  }

  // Temper the relevance score with an exponential curve. Each point of
  // relevance (roughly, each keystroke) is worth less than the last. This means
  // that typing a few characters of a word is enough to promote matches very
  // high, with any subsequent characters being worth comparatively less.
  // TODO(mgiuca): This doesn't really play well with Omnibox results, since as
  // you type more characters, the app/omnibox results tend to jump over each
  // other.
  relevance_ = 1.0 - std::pow(0.5, relevance_);

  return relevance_ > kNoMatchScore;
}

bool TokenizedStringMatch::Calculate(const base::string16& query,
                                     const base::string16& text) {
  const TokenizedString tokenized_query(query);
  const TokenizedString tokenized_text(text);
  return Calculate(tokenized_query, tokenized_text);
}

}  // namespace ash
