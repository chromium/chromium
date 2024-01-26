// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_executor.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using ModelInput = PreloadingModelExecutor::ModelInput;
using ModelOutput = PreloadingModelExecutor::ModelOutput;

class PreloadingModelExecutorTest : public testing::Test {
 public:
  PreloadingModelExecutorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPreloadingHeuristicsMLModel);
  }
  ~PreloadingModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);

    model_file_path_ = source_root_dir.AppendASCII("chrome")
                           .AppendASCII("browser")
                           .AppendASCII("navigation_predictor")
                           .AppendASCII("test")
                           .AppendASCII("preloading_heuristics.tflite");
    execution_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    model_executor_ = std::make_unique<PreloadingModelExecutor>();
    model_executor_->InitializeAndMoveToExecutionThread(
        /*model_inference_timeout=*/std::nullopt,
        optimization_guide::proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING,
        execution_task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    // Destroy model executor.
    execution_task_runner_->DeleteSoon(FROM_HERE, std::move(model_executor_));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::FilePath model_file_path_;
  scoped_refptr<base::SequencedTaskRunner> execution_task_runner_;
  std::unique_ptr<PreloadingModelExecutor> model_executor_;
};

TEST_F(PreloadingModelExecutorTest, ExecuteModel) {
  // Update model file.
  execution_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &optimization_guide::ModelExecutor<ModelOutput,
                                             ModelInput>::UpdateModelFile,
          model_executor_->GetWeakPtrForExecutionThread(), model_file_path_));

  // Execute model.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  base::OnceCallback<void(const std::optional<ModelOutput>&)>
      execution_callback = base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::optional<ModelOutput>& output) {
            ASSERT_TRUE(output.has_value());
            // TODO(isaboori): After the trained model is approved, use
            // realistic inputs and check the output value.
            run_loop->Quit();
          },
          run_loop.get());
  base::TimeTicks now = base::TimeTicks::Now();
  ModelInput input = std::vector<float>(/*count=*/17, /*value=*/0.0);
  execution_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&optimization_guide::ModelExecutor<
                                    ModelOutput, ModelInput>::SendForExecution,
                                model_executor_->GetWeakPtrForExecutionThread(),
                                std::move(execution_callback), now, input));
  run_loop->Run();
}
