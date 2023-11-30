// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_query/visual_query_suggestions_service.h"

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

using companion::visual_query::VisualQuerySuggestionsService;

namespace {

base::FilePath model_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("companion_visual_query")
      .AppendASCII("test-model-quantized.tflite");
}

void GetModelWithMetadataCallback(base::File model,
                                  const std::string& config_proto) {
  EXPECT_TRUE(model.IsValid());
  EXPECT_TRUE(config_proto.empty());
}
}  // namespace

class VisualQuerySuggestionsServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    test_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    service_ = std::make_unique<VisualQuerySuggestionsService>(
        test_model_provider_.get(), background_task_runner);

    absl::optional<optimization_guide::proto::Any> model_metadata;

    model_info_ = optimization_guide::TestModelInfoBuilder()
                      .SetModelFilePath(model_file_path())
                      .SetVersion(123)
                      .Build();

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    service_->Shutdown();
    service_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<companion::visual_query::VisualQuerySuggestionsService>
      service_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider_;
  std::unique_ptr<optimization_guide::ModelInfo> model_info_;
};

TEST_F(VisualQuerySuggestionsServiceTest, OnModelUpdated) {
  VisualQuerySuggestionsService::ModelUpdateCallback callback =
      base::BindOnce(&GetModelWithMetadataCallback);
  service_->RegisterModelUpdateCallback(std::move(callback));
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
                           *model_info_);
  task_environment_.RunUntilIdle();
}

TEST_F(VisualQuerySuggestionsServiceTest,
       OnModelUpdated_BadOptimizationTarget) {
  VisualQuerySuggestionsService::ModelUpdateCallback callback =
      base::BindOnce(&GetModelWithMetadataCallback);
  service_->RegisterModelUpdateCallback(std::move(callback));
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_TEXT_EMBEDDER,
                           *model_info_);
  task_environment_.RunUntilIdle();
}

TEST_F(VisualQuerySuggestionsServiceTest, OnModelUpdated_InvalidModelFile) {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  std::unique_ptr<optimization_guide::ModelInfo> invalid_model_info_ =
      optimization_guide::TestModelInfoBuilder()
          .SetModelFilePath(source_root_dir.AppendASCII("chrome")
                                .AppendASCII("test")
                                .AppendASCII("data")
                                .AppendASCII("companion_visual_query")
                                .AppendASCII("wack-a-doodle.tflite"))
          .SetVersion(123)
          .Build();

  VisualQuerySuggestionsService::ModelUpdateCallback callback =
      base::BindOnce(&GetModelWithMetadataCallback);
  service_->RegisterModelUpdateCallback(std::move(callback));
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
                           *invalid_model_info_);
  task_environment_.RunUntilIdle();
}

TEST_F(VisualQuerySuggestionsServiceTest, OnModelUpdated_ModelAlreadyLoaded) {
  VisualQuerySuggestionsService::ModelUpdateCallback callback =
      base::BindOnce(&GetModelWithMetadataCallback);
  service_->RegisterModelUpdateCallback(std::move(callback));
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
                           *model_info_);
  // Call OnModelUpdated again to instrument closing the model file before
  // reload.
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
                           *model_info_);
  task_environment_.RunUntilIdle();
}

TEST_F(VisualQuerySuggestionsServiceTest, OnModelUpdated_NullModelUpdate) {
  VisualQuerySuggestionsService::ModelUpdateCallback callback =
      base::BindOnce(&GetModelWithMetadataCallback);
  service_->RegisterModelUpdateCallback(std::move(callback));
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
                           *model_info_);
  task_environment_.RunUntilIdle();

  // Null model update should unload the model.
  VisualQuerySuggestionsService::ModelUpdateCallback callback2 =
      base::BindOnce(&GetModelWithMetadataCallback);
  service_->OnModelUpdated(optimization_guide::proto::OptimizationTarget::
                               OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
                           absl::nullopt);
  service_->RegisterModelUpdateCallback(std::move(callback2));
  task_environment_.RunUntilIdle();
}
