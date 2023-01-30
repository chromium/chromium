// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/ftrl_optimizer.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using testing::DoubleEq;
using testing::DoubleNear;
using testing::ElementsAre;

double kEps = 1.0e-5;

}  // namespace

class FtrlOptimizerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  FtrlOptimizer::Proto GetProto() {
    return FtrlOptimizer::Proto(GetPath(), base::Seconds(0));
  }

  FtrlOptimizer::Params TestingParams(size_t num_experts) {
    FtrlOptimizer::Params params;
    params.alpha = 1.0;
    params.gamma = 0.1;
    params.num_experts = num_experts;
    return params;
  }

  void ClearDisk() {
    base::DeleteFile(GetPath());
    ASSERT_FALSE(base::PathExists(GetPath()));
  }

  FtrlOptimizerProto ReadFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    FtrlOptimizerProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void WriteToDisk(const FtrlOptimizerProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void WriteWeightsToDisk(const std::vector<double>& weights) {
    FtrlOptimizerProto proto;
    proto.set_version(1);
    for (double w : weights)
      proto.add_weights(w);
    WriteToDisk(proto);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

// Instantiate and call some methods.
TEST_F(FtrlOptimizerTest, SmokeTest) {
  FtrlOptimizer ftrl(GetProto(), TestingParams(/*num_experts=*/2u));
  ftrl.Score({"abcd"}, {{0.1}, {0.2}});
  ftrl.Train("abcd");
  Wait();
  ftrl.Score({"abcd"}, {{0.1}, {0.2}});
  ftrl.Train("abcd");
}

// Given some weights and scores, check we calculate the weighted average
// correctly.
TEST_F(FtrlOptimizerTest, Score) {
  WriteWeightsToDisk({0.3, 0.7});
  FtrlOptimizer ftrl(GetProto(), TestingParams(/*num_experts=*/2u));
  Wait();

  // Expected scores are:
  // 0.1*0.3 + 0.3*0.7 == 0.24
  // 0.2*0.3 + 0.4*0.7 == 0.34
  EXPECT_THAT(ftrl.Score({"a", "b"}, {{0.1, 0.2}, {0.3, 0.4}}),
              ElementsAre(DoubleEq(0.24), DoubleEq(0.34)));
}

// Check that two experts, one that predicts correctly and one incorrectly, get
// the right score adjustments after training.
TEST_F(FtrlOptimizerTest, Train) {
  WriteWeightsToDisk({0.5, 0.5});
  FtrlOptimizer ftrl(GetProto(), TestingParams(/*num_experts=*/2u));
  Wait();

  ftrl.Score({"a", "b"}, {{0.2, 0.1}, {0.1, 0.2}});
  ftrl.Train("a");
  Wait();

  // Expert one predicted {a, b} and expert two predicted {b, a}. The scores
  // pre-normalization should be:
  double one_score = 0.5;
  double two_score = 0.322939;

  double total = one_score + two_score;
  auto proto = ReadFromDisk();
  EXPECT_THAT(proto.weights()[0], DoubleNear(one_score / total, kEps));
  EXPECT_THAT(proto.weights()[1], DoubleNear(two_score / total, kEps));
}

// Test that a 'good' expert will outweigh a 'bad' expert after several training
// iterations, but that the 'bad' expert can recover if it starts predicting
// accurately.
TEST_F(FtrlOptimizerTest, TrainSeveralTimes) {
  WriteWeightsToDisk({0.5, 0.5});
  FtrlOptimizer ftrl(GetProto(), TestingParams(/*num_experts=*/2u));
  Wait();

  // Do several iterations of training where the first expert is correct.
  for (int i = 0; i < 10; ++i) {
    ftrl.Score({"a", "b", "c", "d"},
               {{1.0, 2.0, 3.0, 4.0}, {4.0, 3.0, 2.0, 1.0}});
    ftrl.Train("d");
  }
  Wait();

  // The first expert should outweigh the second.
  auto proto = ReadFromDisk();
  EXPECT_GT(proto.weights()[0], 0.9);
  EXPECT_LT(proto.weights()[1], 0.1);

  // Do several iterations of training where the second expert is correct.
  for (int i = 0; i < 10; ++i) {
    ftrl.Score({"a", "b", "c", "d"},
               {{1.0, 2.0, 3.0, 4.0}, {4.0, 3.0, 2.0, 1.0}});
    ftrl.Train("a");
  }
  Wait();

  // The second expert should have recovered and outweigh the first.
  proto = ReadFromDisk();
  EXPECT_LT(proto.weights()[0], 0.1);
  EXPECT_GT(proto.weights()[1], 0.9);
}

// Test training when there're more than 1 providers and that items
// from different call of score was selected for training purpose.
TEST_F(FtrlOptimizerTest, TrainWithMultipleProvidersDifferentScore) {
  WriteWeightsToDisk({0.5, 0.5});
  FtrlOptimizer ftrl(GetProto(), TestingParams(/*num_experts=*/2u));
  Wait();

  // Train when the selected result have higher score from the first expert.
  ftrl.Score({"a", "b", "c", "d"},
             {{1.0, 2.0, 3.0, 4.0}, {4.0, 3.0, 2.0, 1.0}});
  ftrl.Score({"e", "f"}, {{1.0, 2.0}, {5.0, 1.0}});
  ftrl.Train("d");
  Wait();

  // The first expert should outweigh the second.
  auto proto = ReadFromDisk();
  EXPECT_GT(proto.weights()[0], 0.52);
  EXPECT_LT(proto.weights()[1], 0.48);

  // Train when the selected result have higher
  // score from the second expert.
  ftrl.Train("e");
  Wait();

  // The second expert should outweigh the first.
  proto = ReadFromDisk();
  EXPECT_LT(proto.weights()[0], 0.49);
  EXPECT_GT(proto.weights()[1], 0.51);
}

// Test if score has been successfully override.
TEST_F(FtrlOptimizerTest, TrainWithMultipleProvidersOverrideScore) {
  WriteWeightsToDisk({0.5, 0.5});
  FtrlOptimizer ftrl(GetProto(), TestingParams(/*num_experts=*/2u));
  Wait();

  // Call score twice with same items, the latest one score call
  // should override the older one.
  ftrl.Score({"a", "b", "c", "d"},
             {{1.0, 2.0, 3.0, 4.0}, {4.0, 3.0, 2.0, 1.0}});
  ftrl.Score({"a", "b", "c", "d"},
             {{4.0, 3.0, 2.0, 1.0}, {1.0, 2.0, 3.0, 4.0}});
  ftrl.Train("d");
  Wait();

  // The second expert should outweigh the first.
  auto proto = ReadFromDisk();
  EXPECT_LT(proto.weights()[0], 0.35);
  EXPECT_GT(proto.weights()[1], 0.65);
}

// Test if the last_expert_scores_ is empty after calling Clear.
TEST_F(FtrlOptimizerTest, Clear) {
  WriteWeightsToDisk({0.5, 0.5});
  FtrlOptimizer ftrl(GetProto(), TestingParams(/*num_experts=*/2u));
  Wait();

  // Call Clear() after calling Score(). Train should have no effect on weights.
  ftrl.Score({"a", "b", "c", "d"},
             {{1.0, 2.0, 3.0, 4.0}, {4.0, 3.0, 2.0, 1.0}});
  ftrl.Clear();
  ftrl.Train("d");
  Wait();

  // The weight should not change since the last_expert_scores_ is empty.
  // The loss should return 0.
  auto proto = ReadFromDisk();
  EXPECT_EQ(proto.weights()[0], 0.5);
  EXPECT_EQ(proto.weights()[1], 0.5);

  // Call the Clear() after training.
  ftrl.Score({"a", "b", "c", "d"},
             {{1.0, 2.0, 3.0, 4.0}, {4.0, 3.0, 2.0, 1.0}});
  ftrl.Train("d");
  ftrl.Clear();
  Wait();

  // The weights should be trained normally.
  proto = ReadFromDisk();
  EXPECT_GT(proto.weights()[0], 0.65);
  EXPECT_LT(proto.weights()[1], 0.35);
}

}  // namespace app_list::test
