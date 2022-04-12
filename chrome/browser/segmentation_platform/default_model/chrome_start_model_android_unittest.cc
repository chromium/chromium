// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ChromeStartModelTest : public testing::Test {
 public:
  ChromeStartModelTest() = default;
  ~ChromeStartModelTest() override = default;

  void SetUp() override {
    chrome_start_model_ = std::make_unique<ChromeStartModel>();
  }

  void TearDown() override {
    chrome_start_model_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    chrome_start_model_->InitAndFetchModel(
        base::BindRepeating(&ChromeStartModelTest::OnInitFinishedCallback,
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
    chrome_start_model_->ExecuteModelWithInput(
        inputs,
        base::BindOnce(&ChromeStartModelTest::OnExecutionFinishedCallback,
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
  std::unique_ptr<ChromeStartModel> chrome_start_model_;
};

TEST_F(ChromeStartModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ChromeStartModelTest, ExecuteModelWithInput) {
  const unsigned kMvIndex = 3;

  std::vector<float> input = {0.3, 1.4, -2, 4, 1, 6, 7, 8};

  input[kMvIndex] = 0;
  ExpectExecutionWithInput(input, false, 0);

  input[kMvIndex] = -3;
  ExpectExecutionWithInput(input, false, 0);

  input[kMvIndex] = 1;
  ExpectExecutionWithInput(input, false, 1);

  input[kMvIndex] = 4.6;
  ExpectExecutionWithInput(input, false, 1);

  ExpectExecutionWithInput({}, true, 0);
  ExpectExecutionWithInput({1, 2, 3, 4, 5, 6, 7, 8, 9}, true, 0);
}

}  // namespace segmentation_platform
