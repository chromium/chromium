// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/feed_user_segment.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

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
    std::move(closure).Run();
  }

  void ExpectExecutionWithInput(const std::vector<float>& inputs,
                                bool expected_error,
                                float expected_result) {
    base::RunLoop loop;
    feed_user_model_->ExecuteModelWithInput(
        inputs, base::BindOnce(&FeedUserModelTest::OnExecutionFinishedCallback,
                               base::Unretained(this), loop.QuitClosure(),
                               expected_error, expected_result));
    loop.Run();
  }

  void OnExecutionFinishedCallback(base::RepeatingClosure closure,
                                   bool expected_error,
                                   float expected_result,
                                   const absl::optional<float>& result) {
    if (expected_error) {
      EXPECT_FALSE(result.has_value());
    } else {
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), expected_result);
    }
    std::move(closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FeedUserSegment> feed_user_model_;
};

TEST_F(FeedUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(FeedUserModelTest, ExecuteModelWithInput) {
  std::vector<float> input(9, 0);

  ExpectExecutionWithInput(input, false, 0);

  input[4] = 3;
  input[5] = 2;
  ExpectExecutionWithInput(input, false, 0.5);

  input[3] = 3;
  ExpectExecutionWithInput(input, false, 0.75);

  input[0] = 1;
  input[2] = 2;
  ExpectExecutionWithInput(input, false, 1);

  ExpectExecutionWithInput({}, true, 0);
  ExpectExecutionWithInput({1, 2}, true, 0);
}

}  // namespace segmentation_platform
