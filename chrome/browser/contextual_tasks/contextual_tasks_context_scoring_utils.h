// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// Represents a passage of text from a tab and its relevance score.
// The score is computed using the cosine similarity between the query's
// embedding and the passage's embedding.
struct ScoredPassage {
  float score = 0.0f;
  std::string text;
};

struct TabSimilarityScores {
 TabSimilarityScores();
 ~TabSimilarityScores();
 TabSimilarityScores(const TabSimilarityScores&);
 TabSimilarityScores& operator=(const TabSimilarityScores&);

 ScoredPassage best;
 ScoredPassage worst = {1.0f, ""};
};

struct QueryStateSignals {
 QueryStateSignals();
 QueryStateSignals(const QueryStateSignals&) = delete;
 QueryStateSignals& operator=(const QueryStateSignals&) = delete;
 QueryStateSignals(QueryStateSignals&&);
 QueryStateSignals& operator=(QueryStateSignals&&);
 ~QueryStateSignals();

 int query_word_count = 0;
 float query_active_tab_title_similarity = 0.0f;
 std::vector<ScoredPassage> query_active_tab_passage_similarities;
};

struct TabSignals {
 TabSignals();
 TabSignals(const TabSignals&) = delete;
 TabSignals& operator=(const TabSignals&) = delete;
 TabSignals(TabSignals&&);
 TabSignals& operator=(TabSignals&&);
 ~TabSignals();

 base::WeakPtr<content::WebContents> web_contents;

 // TODO(b/462793437): Remove embedding_score once the migration to
 // query_candidate_tab_similarity vector is complete.
 std::optional<float> embedding_score;

 // Lexical signals.
 int num_query_title_matching_words = 0;

 // Similarity scores for the query and the candidate tab (title + passages).
 float query_candidate_tab_title_similarity = 0.0f;
 std::vector<ScoredPassage> query_candidate_tab_passage_similarities;

 // Used for debugging/logging.
 std::optional<TabSimilarityScores> similarity_scores;

 // Similarity between the active tab title and the candidate tab title.
 float active_title_candidate_title_similarity = 0.0f;

 // Dynamic (engagement) signals.
 std::optional<base::TimeDelta> duration_since_last_active;
 std::optional<base::TimeDelta> duration_of_last_visit;
};

// Gets the score for a tab based only on the static signals.
double GetScoreWithStaticSignals(const TabSignals& signals);

// Gets the score for a tab based on all signals.
double GetScoreWithAllSignals(const TabSignals& signals);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_
