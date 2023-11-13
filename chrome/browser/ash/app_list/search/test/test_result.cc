// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/test_result.h"

namespace app_list {

TestResult::TestResult(const std::string& id,
                       ResultType result_type,
                       Category category,
                       double display_score,
                       double normalized_relevance) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  SetResultType(result_type);
  SetCategory(category);
  SetDisplayScore(display_score);
  scoring().set_normalized_relevance(normalized_relevance);
}

TestResult::TestResult(const std::string& id,
                       double relevance,
                       double normalized_relevance,
                       DisplayType display_type,
                       bool best_match) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  set_relevance(relevance);
  scoring().set_normalized_relevance(normalized_relevance);
  SetDisplayType(display_type);
  SetBestMatch(best_match);
}

TestResult::TestResult(const std::string& id,
                       DisplayType display_type,
                       Category category,
                       int best_match_rank,
                       double relevance,
                       double ftrl_result_score) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  SetDisplayType(display_type);
  SetCategory(category);
  scoring().set_best_match_rank(best_match_rank);
  set_relevance(relevance);
  scoring().set_ftrl_result_score(ftrl_result_score);
}

TestResult::TestResult(const std::string& id,
                       ResultType result_type,
                       crosapi::mojom::SearchResult::AnswerType answer_type,
                       DisplayType display_type) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  SetResultType(result_type);
  set_answer_type(answer_type);
  SetDisplayType(display_type);
}

TestResult::TestResult(const std::string& id,
                       double relevance,
                       double normalized_relevance,
                       MetricsType metrics_type) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(id));
  set_relevance(relevance);
  scoring().set_normalized_relevance(normalized_relevance);
  SetMetricsType(metrics_type);
}

TestResult::TestResult(const std::string& id,
                       DisplayType display_type,
                       Category category,
                       const std::string& fileName,
                       const std::string& path,
                       int best_match_rank,
                       double relevance,
                       double ftrl_result_score) {
  set_id(id);
  SetTitle(base::UTF8ToUTF16(fileName));
  SetDisplayType(display_type);
  SetCategory(category);
  SetFilePath(base::FilePath(path));
  scoring().set_best_match_rank(best_match_rank);
  set_relevance(relevance);
  scoring().set_ftrl_result_score(ftrl_result_score);
}

TestResult::~TestResult() = default;

}  // namespace app_list
