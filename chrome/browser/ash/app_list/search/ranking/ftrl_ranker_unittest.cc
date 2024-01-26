// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/ftrl_ranker.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/ranking_test_util.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using testing::ElementsAre;

}  // namespace

class TestRanker : public Ranker {
 public:
  TestRanker() = default;
  ~TestRanker() override = default;

  void SetNextScores(const std::vector<double>& scores) {
    next_scores_ = scores;
  }

  // Ranker:
  std::vector<double> GetResultRanks(const ResultsMap& results,
                                     ProviderType provider) override {
    return next_scores_;
  }

  std::vector<double> GetCategoryRanks(const ResultsMap& results,
                                       const CategoriesList& categories,
                                       ProviderType provider) override {
    return next_scores_;
  }

 private:
  std::vector<double> next_scores_;
};

// FtrlRankerTest --------------------------------------------------------------

class FtrlRankerTest : public RankerTestBase {
 public:
  FtrlOptimizer::Params TestingParams(size_t num_experts) {
    FtrlOptimizer::Params params;
    params.alpha = 1.0;
    params.gamma = 0.1;
    params.num_experts = num_experts;
    return params;
  }
};

TEST_F(FtrlRankerTest, TrainAndRankResults) {
  // Set up an FTRL result ranker with two experts that statically return scores
  // for four items.
  auto good_ranker = std::make_unique<TestRanker>();
  auto bad_ranker = std::make_unique<TestRanker>();
  good_ranker->SetNextScores({4.0, 3.0, 2.0, 1.0});
  bad_ranker->SetNextScores({1.0, 2.0, 3.0, 4.0});

  FtrlRanker ranker(FtrlRanker::RankingKind::kResults, TestingParams(2),
                    FtrlOptimizer::Proto(GetPath(), base::Seconds(0)));
  ranker.AddExpert(std::move(good_ranker));
  ranker.AddExpert(std::move(bad_ranker));
  Wait();

  // Make four results and mimic several rank/train cycles where we always
  // launch "a", so the good/bad rankers always performs well/poorly.
  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeResults({"a", "b", "c", "d"});

  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("a"));
  }

  // The weights of the FTRL optimizer should reflect that the good ranker is
  // better than the bad ranker.
  Wait();
  auto proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2);
  EXPECT_GE(proto.weights()[0], 0.9);
  EXPECT_LE(proto.weights()[1], 0.1);

  // Now reverse the situation: change the clicked result so the bad ranker is
  // now performing well.
  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("d"));
  }

  // The weights of the 'bad' expert should have recovered.
  Wait();
  proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2);
  EXPECT_LE(proto.weights()[0], 0.1);
  EXPECT_GE(proto.weights()[1], 0.9);
}

TEST_F(FtrlRankerTest, TrainAndRankResultsWithMultipleProviders) {
  // Set up an FTRL result ranker with two experts that statically return scores
  // for eight items.
  auto good_ranker = std::make_unique<TestRanker>();
  auto bad_ranker = std::make_unique<TestRanker>();
  good_ranker->SetNextScores({1.0, 2.0, 3.0, 4.0});
  bad_ranker->SetNextScores({4.0, 3.0, 2.0, 1.0});

  FtrlRanker ranker(FtrlRanker::RankingKind::kResults, TestingParams(2),
                    FtrlOptimizer::Proto(GetPath(), base::Seconds(0)));
  ranker.AddExpert(std::move(good_ranker));
  ranker.AddExpert(std::move(bad_ranker));
  Wait();

  // Make four results within each provider and mimic several rank/train cycles
  // where we always launch "h" from kFileSearch provider, so the good/bad
  // ranker always performs well poorly.
  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeResults({"a", "b", "c", "d"});
  results[ResultType::kFileSearch] = MakeResults({"e", "f", "g", "h"});

  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.UpdateResultRanks(results, ResultType::kFileSearch);
    ranker.Train(MakeLaunchData("h"));
  }

  // The weights of the FTRL optimizer should reflect that good_ranker is
  // better than the bad_ranker and that Train() apply to results from both
  // providers.
  Wait();
  auto proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2);
  EXPECT_GE(proto.weights()[0], 0.9);
  EXPECT_LE(proto.weights()[1], 0.1);

  // Now reverse the situation: change the clicked result so bad_ranker is
  // now performing well.
  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.UpdateResultRanks(results, ResultType::kFileSearch);
    ranker.Train(MakeLaunchData("a"));
  }

  // The weights of the 'bad' expert should have recovered.
  Wait();
  proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2);
  EXPECT_LE(proto.weights()[0], 0.1);
  EXPECT_GE(proto.weights()[1], 0.9);
}

TEST_F(FtrlRankerTest, TrainAndRankCategories) {
  auto good_ranker = std::make_unique<TestRanker>();
  auto bad_ranker = std::make_unique<TestRanker>();
  good_ranker->SetNextScores({4.0, 3.0, 2.0, 1.0});
  bad_ranker->SetNextScores({1.0, 2.0, 3.0, 4.0});

  FtrlRanker ranker(FtrlRanker::RankingKind::kCategories, TestingParams(2),
                    FtrlOptimizer::Proto(GetPath(), base::Seconds(0)));
  ranker.AddExpert(std::move(good_ranker));
  ranker.AddExpert(std::move(bad_ranker));
  Wait();

  // Make results in three categories (assistant has no results) and mimic
  // several rank/train cycles where we always launch "a", so the good/bad
  // rankers always performs well/poorly.
  ResultsMap results;
  results[ResultType::kInstalledApp] =
      MakeResults({"a"}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kOsSettings] =
      MakeResults({"b"}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kFileSearch] =
      MakeResults({"c"}, ResultType::kFileSearch, Category::kFiles);
  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kSettings},
                             {.category = Category::kSearchAndAssistant},
                             {.category = Category::kFiles}});

  for (int i = 0; i < 10; ++i) {
    ranker.UpdateCategoryRanks(results, categories, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("a", Category::kApps));
  }

  // The weights of the FTRL optimizer should reflect that the good ranker is
  // better than the bad ranker.
  Wait();
  auto proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2);
  EXPECT_GE(proto.weights()[0], 0.9);
  EXPECT_LE(proto.weights()[1], 0.1);

  // Now reverse the situation: change the clicked result so the bad ranker is
  // now performing well. Train for longer because neither ranker does a
  // particularly good job at predicting "c"'s category.
  for (int i = 0; i < 10; ++i) {
    ranker.UpdateResultRanks(results, ResultType::kInstalledApp);
    ranker.Train(MakeLaunchData("c", Category::kFiles));
  }

  // The weights of the 'bad' expert should have recovered.
  Wait();
  proto = ReadProtoFromDisk<FtrlOptimizerProto>();
  ASSERT_EQ(proto.weights_size(), 2);
  EXPECT_LE(proto.weights()[0], 0.1);
  EXPECT_GE(proto.weights()[1], 0.9);
}

// ResultScoringShim -------------------------------------------------

class ResultScoringShimTest : public RankerTestBase {};

TEST_F(ResultScoringShimTest, Rank) {
  ResultScoringShim ranker(
      ResultScoringShim::ScoringMember::kNormalizedRelevance);

  auto results = MakeResults({"a", "b", "c"});
  ASSERT_EQ(results.size(), 3u);
  results[0]->scoring().set_normalized_relevance(0.2);
  results[1]->scoring().set_normalized_relevance(0.5);
  results[2]->scoring().set_normalized_relevance(0.1);

  ResultsMap results_map;
  results_map[ResultType::kInstalledApp] = std::move(results);

  auto scores = ranker.GetResultRanks(results_map, ResultType::kInstalledApp);
  EXPECT_EQ(scores[0], 0.2);
  EXPECT_EQ(scores[1], 0.5);
  EXPECT_EQ(scores[2], 0.1);
}

// BestResultCategoryRanker ----------------------------------------------------

class BestResultCategoryRankerTest : public RankerTestBase {};

TEST_F(BestResultCategoryRankerTest, Rank) {
  BestResultCategoryRanker ranker;

  ResultsMap results;
  results[ResultType::kInstalledApp] = MakeScoredResults(
      {"a", "b"}, {0.1, 0.5}, ResultType::kInstalledApp, Category::kApps);
  results[ResultType::kOsSettings] = MakeScoredResults(
      {"c", "d"}, {0.3, 0.3}, ResultType::kOsSettings, Category::kSettings);
  results[ResultType::kFileSearch] = MakeScoredResults(
      {"e", "f"}, {0.3, 0.8}, ResultType::kFileSearch, Category::kFiles);
  CategoriesList categories({{.category = Category::kApps},
                             {.category = Category::kSettings},
                             {.category = Category::kFiles}});

  ranker.Start(u"query", categories);

  // Only the app category should be scored, because that's the only one to have
  // returned.
  auto scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kInstalledApp);
  ASSERT_EQ(scores.size(), 3u);
  EXPECT_EQ(scores[0], 0.5);
  EXPECT_EQ(scores[1], 0.0);
  EXPECT_EQ(scores[2], 0.0);

  // Now all categories should be scored.
  ranker.GetCategoryRanks(results, categories, ResultType::kOsSettings);
  scores =
      ranker.GetCategoryRanks(results, categories, ResultType::kFileSearch);
  ASSERT_EQ(scores.size(), 3u);
  EXPECT_EQ(scores[0], 0.5);
  EXPECT_EQ(scores[1], 0.3);
  EXPECT_EQ(scores[2], 0.8);
}

}  // namespace app_list::test
