// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const char kModelFilename[] = "visual_model.tflite";

}  // namespace

class VisualSearchSuggestionsServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    test_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    service_ = std::make_unique<
        companion::visual_search::VisualSearchSuggestionsService>(
        test_model_provider_.get(), background_task_runner);

    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("components/test/data");

    base::flat_set<base::FilePath> additional_files;
    additional_files.insert(test_data_dir.AppendASCII(kModelFilename));

    model_info_ =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(test_data_dir.AppendASCII(kModelFilename))
            .SetAdditionalFiles(additional_files)
            .SetVersion(123)
            .Build();

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    service_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<companion::visual_search::VisualSearchSuggestionsService>
      service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider_;
  std::unique_ptr<optimization_guide::ModelInfo> model_info_;
};

TEST_F(VisualSearchSuggestionsServiceTest, OnModelUpdated) {
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
                           *model_info_);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(service_->GetModelFile().IsValid());
}
