// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_RANKING_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_RANKING_TEST_UTIL_H_

#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class RankerTestBase : public testing::Test {
 public:
  void SetUp() override;

  base::FilePath GetPath();

  // A helper function for creating results. For convenience, the provided
  // scores are set as both the display score and normalized relevance.
  Results MakeScoredResults(const std::vector<std::string>& ids,
                            const std::vector<double> scores,
                            ResultType result_type = ResultType::kUnknown,
                            Category category = Category::kUnknown);

  // A helper function for creating results, for when results don't need scores.
  Results MakeResults(const std::vector<std::string>& ids,
                      ResultType result_type = ResultType::kUnknown,
                      Category category = Category::kUnknown);

  LaunchData MakeLaunchData(const std::string& id,
                            Category category = Category::kUnknown);

  template <typename T>
  T ReadProtoFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    T proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  // A helper function that set ftrl_score for the given results from one
  // provider.
  void SetFtrlScore(const ResultsMap& results,
                    ProviderType provider,
                    const std::vector<double> ftrl_scores);
  void Wait();

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_RANKING_TEST_UTIL_H_
