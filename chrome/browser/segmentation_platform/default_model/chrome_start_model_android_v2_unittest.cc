// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android_v2.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ChromeStartModelV2Test : public testing::Test {
 public:
  ChromeStartModelV2Test() = default;
  ~ChromeStartModelV2Test() override = default;

  void SetUp() override {
    chrome_start_model_ = std::make_unique<ChromeStartModelV2>();
  }

  void TearDown() override {
    chrome_start_model_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExpectInitAndFetchModel() {
    base::RunLoop loop;
    chrome_start_model_->InitAndFetchModel(
        base::BindRepeating(&ChromeStartModelV2Test::OnInitFinishedCallback,
                            base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnInitFinishedCallback(base::RepeatingClosure closure,
                              proto::SegmentId target,
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
        base::BindOnce(&ChromeStartModelV2Test::OnExecutionFinishedCallback,
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
  std::unique_ptr<ChromeStartModelV2> chrome_start_model_;
};

TEST_F(ChromeStartModelV2Test, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ChromeStartModelV2Test, ExecuteModelWithInput) {
  // 3 input features definde in `kChromeStartUMAFeatures`, set all to 0.
  std::vector<float> input = {0, 0, 0};
  const float kDefaultReturnTimeSeconds = 28800;
  ExpectExecutionWithInput(input, false, kDefaultReturnTimeSeconds);

  // Set to higher values, the model returns the same result.
  input = {3, 6, 3};
  ExpectExecutionWithInput(input, false, kDefaultReturnTimeSeconds);
}

}  // namespace segmentation_platform
