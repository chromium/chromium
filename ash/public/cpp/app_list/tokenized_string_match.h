// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_TOKENIZED_STRING_MATCH_H_
#define ASH_PUBLIC_CPP_APP_LIST_TOKENIZED_STRING_MATCH_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/range/range.h"

namespace ash {

class TokenizedString;

// TokenizedStringMatch takes two tokenized strings: one as the text and
// the other one as the query. It matches the query against the text,
// calculates a relevance score between [0, 1] and marks the matched portions
// of text. A relevance of zero means the two are completely different to each
// other. The higher the relevance score, the better the two strings are
// matched. Matched portions of text are stored as index ranges.
class ASH_PUBLIC_EXPORT TokenizedStringMatch {
 public:
  typedef std::vector<gfx::Range> Hits;

  TokenizedStringMatch();
  ~TokenizedStringMatch();

  // Calculates the relevance and hits. Returns true if the two strings are
  // somewhat matched, i.e. relevance score is not zero.
  bool Calculate(const TokenizedString& query, const TokenizedString& text);

  // Convenience wrapper to calculate match from raw string input.
  bool Calculate(const base::string16& query, const base::string16& text);

  double relevance() const { return relevance_; }
  const Hits& hits() const { return hits_; }

 private:
  // Score in range of [0,1] representing how well the query matches the text.
  double relevance_;

  // Char index ranges in |text| of where matches are found.
  Hits hits_;

  DISALLOW_COPY_AND_ASSIGN(TokenizedStringMatch);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_TOKENIZED_STRING_MATCH_H_
