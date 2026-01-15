// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SCORING_UTILS_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

struct TabSignals {
  raw_ptr<content::WebContents> web_contents = nullptr;
  // Static signals.
  std::optional<float> embedding_score;
  std::optional<int> num_query_title_matching_words;
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
