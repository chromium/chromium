// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_RESULT_H_

#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

class TestResult : public ChromeSearchResult {
 public:
  // TestResult is used by many test suites. Each test suite operates on
  // different members of ChromeSearchResult. This set of constructors aims to
  //
  // (a) generalize across the use cases (to minimize constructor number) and
  // (b) retain flexibility (to keep points-of-use from becoming cumbersome).
  TestResult() = default;

  explicit TestResult(const std::string& id,
                      ResultType result_type = ResultType::kUnknown,
                      Category category = Category::kUnknown,
                      double display_score = 0.0,
                      double normalized_relevance = 0.0);

  TestResult(const std::string& id,
             double relevance,
             double normalized_relevance = 0.0,
             DisplayType display_type = DisplayType::kNone,
             bool best_match = false);

  TestResult(const std::string& id,
             DisplayType display_type,
             Category category,
             int best_match_rank,
             double relevance,
             double ftrl_result_score);

  TestResult(const std::string& id,
             ResultType result_type,
             crosapi::mojom::SearchResult::AnswerType answer_type,
             DisplayType display_type);

  TestResult(const std::string& id,
             double relevance,
             double normalized_relevance,
             MetricsType metrics_type = MetricsType::NO_RESULT);

  // File result
  TestResult(const std::string& id,
             DisplayType display_type,
             Category category,
             const std::string& fileName,
             const std::string& path,
             int best_match_rank,
             double relevance,
             double ftrl_result_score);

  ~TestResult() override;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_RESULT_H_
