// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/score_normalizing_ranker.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

Results MakeResults(const std::vector<std::string>& ids,
                    const std::vector<double> scores,
                    ResultType result_type) {
  Results res;
  CHECK_EQ(ids.size(), scores.size());
  for (size_t i = 0; i < ids.size(); ++i) {
    auto result =
        std::make_unique<TestResult>(ids[i], result_type, Category::kUnknown,
                                     /*display_score=*/scores[i]);
    result->set_relevance(scores[i]);
    res.push_back(std::move(result));
  }
  return res;
}

}  // namespace

class ScoreNormalizingRankerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  ash::PersistentProto<ScoreNormalizerProto> GetProto() {
    return ash::PersistentProto<ScoreNormalizerProto>(GetPath(),
                                                      base::Seconds(0));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

TEST_F(ScoreNormalizingRankerTest, UpdateResultRanks) {
  ScoreNormalizingRanker ranker(ScoreNormalizer::Params(), GetProto());
  Wait();

  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeResults(
      {"a", "b", "c", "d", "e", "f", "g"},
      {0.4, 0.2, 0.3, 0.25, 0.35, 0.1, 0.33}, ResultType::kInstalledApp);
  results[ResultType::kFileSearch] =
      MakeResults({"h", "i"}, {1.0, 2.0}, ResultType::kFileSearch);

  ranker.UpdateResultRanks(results, ResultType::kInstalledApp);

  // App results should have been scored, but their precise scoring is up to the
  // implementation of the score normalizer itself. Let its tests cover this,
  // and just check that the results have been scored at all, and the results
  // are ordered by relevance after normalization.
  const auto& app_results = results[ResultType::kInstalledApp];
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_GT(app_results[i]->scoring().normalized_relevance(), 0.0);
  }

  std::vector<ChromeSearchResult*> ordered_app_results;
  for (const auto& result : app_results) {
    ordered_app_results.push_back(result.get());
  }

  std::sort(ordered_app_results.begin(), ordered_app_results.end(),
            [](const auto& r1, const auto& r2) {
              return r1->scoring().normalized_relevance() >
                     r2->scoring().normalized_relevance();
            });

  std::vector<std::string> ordered_app_result_ids;
  for (const auto* result : ordered_app_results) {
    ordered_app_result_ids.push_back(result->id());
  }

  EXPECT_EQ(std::vector<std::string>({"a", "e", "g", "c", "d", "b", "f"}),
            ordered_app_result_ids);

  // File results should not have been scored.
  const auto& file_results = results[ResultType::kFileSearch];
  EXPECT_FLOAT_EQ(file_results[0]->scoring().normalized_relevance(), 0.0);
  EXPECT_FLOAT_EQ(file_results[1]->scoring().normalized_relevance(), 0.0);
}

}  // namespace app_list::test
