// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/feed_user_segment.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

// TODO(ssid): Use metadata_utils or share common code for this function.
int ConvertToDiscreteScore(const std::string& mapping_key,
                           float input_score,
                           const proto::SegmentationModelMetadata& metadata) {
  auto iter = metadata.discrete_mappings().find(mapping_key);
  if (iter == metadata.discrete_mappings().end()) {
    iter =
        metadata.discrete_mappings().find(metadata.default_discrete_mapping());
    if (iter == metadata.discrete_mappings().end())
      return 0;
  }
  DCHECK(iter != metadata.discrete_mappings().end());

  const auto& mapping = iter->second;

  // Iterate over the entries and find the largest entry whose min result is
  // equal to or less than the input.
  int discrete_result = 0;
  float largest_score_below_input_score = std::numeric_limits<float>::min();
  for (int i = 0; i < mapping.entries_size(); i++) {
    const auto& entry = mapping.entries(i);
    if (entry.min_result() <= input_score &&
        entry.min_result() > largest_score_below_input_score) {
      largest_score_below_input_score = entry.min_result();
      discrete_result = entry.rank();
    }
  }

  return discrete_result;
}

}  // namespace

class FeedUserModelTest : public testing::Test {
 public:
  FeedUserModelTest() = default;
  ~FeedUserModelTest() override = default;

  void SetUp() override {
    feed_user_model_ = std::make_unique<FeedUserSegment>();
  }

  void TearDown() override {
    feed_user_model_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    feed_user_model_->InitAndFetchModel(
        base::BindRepeating(&FeedUserModelTest::OnInitFinishedCallback,
                            base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnInitFinishedCallback(
      base::RepeatingClosure closure,
      optimization_guide::proto::OptimizationTarget target,
      proto::SegmentationModelMetadata metadata,
      int64_t) {
    EXPECT_EQ(metadata_utils::ValidateMetadataAndFeatures(metadata),
              metadata_utils::ValidationResult::kValidationSuccess);
    fetched_metadata_ = metadata;
    std::move(closure).Run();
  }

  absl::optional<float> ExpectExecutionWithInput(
      const std::vector<float>& inputs) {
    absl::optional<float> result;
    base::RunLoop loop;
    feed_user_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&FeedUserModelTest::OnExecutionFinishedCallback,
                       base::Unretained(this), loop.QuitClosure(), &result));
    loop.Run();
    return result;
  }

  void OnExecutionFinishedCallback(base::RepeatingClosure closure,
                                   absl::optional<float>* output,
                                   const absl::optional<float>& result) {
    *output = result;
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FeedUserSegment> feed_user_model_;
  absl::optional<proto::SegmentationModelMetadata> fetched_metadata_;
};

TEST_F(FeedUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(FeedUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  std::vector<float> input(9, 0);

  absl::optional<float> result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("NoNTPOrHomeOpened",
            FeedUserSegment::GetSubsegmentName(ConvertToDiscreteScore(
                "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  input[4] = 3;
  input[5] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("UsedNtpWithoutModules",
            FeedUserSegment::GetSubsegmentName(ConvertToDiscreteScore(
                "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  input[3] = 3;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("MvtOnly",
            FeedUserSegment::GetSubsegmentName(ConvertToDiscreteScore(
                "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  input[0] = 1;
  input[2] = 2;
  result = ExpectExecutionWithInput(input);
  ASSERT_TRUE(result);
  EXPECT_EQ("ActiveOnFeedAndNtpFeatures",
            FeedUserSegment::GetSubsegmentName(ConvertToDiscreteScore(
                "feed_user_segment_subsegment", *result, *fetched_metadata_)));

  EXPECT_FALSE(ExpectExecutionWithInput({}));
  EXPECT_FALSE(ExpectExecutionWithInput({1, 2}));
}

}  // namespace segmentation_platform
