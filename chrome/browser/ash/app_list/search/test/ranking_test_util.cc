// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/ranking_test_util.h"

#include "chrome/browser/ash/app_list/search/test/test_result.h"

namespace app_list {

void RankerTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

base::FilePath RankerTestBase::GetPath() {
  return temp_dir_.GetPath().Append("proto");
}

Results RankerTestBase::MakeScoredResults(const std::vector<std::string>& ids,
                                          const std::vector<double> scores,
                                          ResultType result_type,
                                          Category category) {
  Results res;
  CHECK_EQ(ids.size(), scores.size());
  for (size_t i = 0; i < ids.size(); ++i) {
    res.push_back(std::make_unique<TestResult>(
        ids[i], result_type, category, /*display_score=*/scores[i],
        /*normalized_relevance=*/scores[i]));
  }
  return res;
}

Results RankerTestBase::MakeResults(const std::vector<std::string>& ids,
                                    ResultType result_type,
                                    Category category) {
  return RankerTestBase::MakeScoredResults(ids, std::vector<double>(ids.size()),
                                           result_type, category);
}

LaunchData RankerTestBase::MakeLaunchData(const std::string& id,
                                          Category category) {
  LaunchData launch;
  launch.launched_from = ash::AppListLaunchedFrom::kLaunchedFromSearchBox;
  launch.id = id;
  launch.category = category;
  return launch;
}

void RankerTestBase::SetFtrlScore(const ResultsMap& results,
                                  ProviderType provider,
                                  const std::vector<double> ftrl_scores) {
  const auto it = results.find(provider);
  ASSERT_NE(it, results.end());

  ASSERT_EQ(it->second.size(), ftrl_scores.size());

  for (size_t i = 0; i < ftrl_scores.size(); i++) {
    (it->second)[i]->scoring().set_ftrl_result_score(ftrl_scores[i]);
  }
}

void RankerTestBase::Wait() {
  task_environment_.RunUntilIdle();
}

}  // namespace app_list
