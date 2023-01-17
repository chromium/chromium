// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_CONSTANTS_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_CONSTANTS_H_

namespace app_list {

// The maximum number of omnibox results to display if we have more results than
// can fit in the UI.
constexpr int kMaxOmniboxResults = 3;

// The maximum number of best matches to show.
constexpr size_t kNumBestMatches = 3u;

// The number of top-ranked best match results to stabilize during the
// post-burn-in period. Stabilized results retain their rank and are not
// displaced by later-arriving results
constexpr size_t kNumBestMatchesToStabilize = 1u;

// The score threshold before we consider a result a best match.
constexpr double kBestMatchThreshold = 0.8;

// The score threshold used when there's keyword ranking.
// This is given by tanh(2.65 * 0.8) where 0.8 is original best match threshold.
constexpr double kBestMatchThresholdWithKeywordRanking = 0.97159407725;

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_CONSTANTS_H_
