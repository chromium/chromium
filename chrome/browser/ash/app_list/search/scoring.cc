// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/scoring.h"

#include <algorithm>

#include "base/strings/stringprintf.h"

namespace app_list {

double Scoring::FinalScore() const {
  if (filter && !override_filter_for_test)
    return -1.0;
  return ftrl_result_score;
}

double Scoring::BestMatchScore() const {
  if (filter)
    return -1.0;
  else
    return std::max(mrfu_result_score, normalized_relevance);
}

::std::ostream& operator<<(::std::ostream& os, const Scoring& scoring) {
  if (scoring.filter)
    return os << "{" << scoring.FinalScore() << " | filtered}";
  return os << base::StringPrintf(
             "{%.2f | nr:%.2f rs:%.2f bm:%d cr:%d bi:%d}", scoring.FinalScore(),
             scoring.normalized_relevance, scoring.ftrl_result_score,
             scoring.best_match_rank, scoring.continue_rank,
             scoring.burnin_iteration);
}

}  // namespace app_list
