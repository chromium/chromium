// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class ModelValidatorBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool /*is_model_valid*/> {
 public:
  ModelValidatorBrowserTest() = default;
  ~ModelValidatorBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {optimization_guide::features::kOptimizationHints, {}},
            {optimization_guide::features::kOptimizationGuideModelDownloading,
             {{"unrestricted_model_downloading", "true"}}},
        },
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    base::FilePath model_file_path;
    if (is_model_valid()) {
      base::PathService::Get(base::DIR_SOURCE_ROOT, &model_file_path);
      model_file_path = model_file_path.AppendASCII("components")
                            .AppendASCII("test")
                            .AppendASCII("data")
                            .AppendASCII("optimization_guide")
                            .AppendASCII("simple_test.tflite");
    } else {
      EXPECT_TRUE(model_dir_.CreateUniqueTempDir());
      model_file_path =
          model_dir_.GetPath().AppendASCII("invalid_model.tflite");
      base::WriteFile(model_file_path, "INVALID MODEL DATA");
    }
    cmd->AppendSwitch(optimization_guide::switches::kModelValidate);
    cmd->AppendSwitchASCII(
        "optimization-guide-model-override",
        "OPTIMIZATION_TARGET_MODEL_VALIDATION:" +
            optimization_guide::FilePathToString(model_file_path));
  }

  // Returns whether the test is validating an valid model.
  bool is_model_valid() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir model_dir_;
};

#if !BUILDFLAG(BUILD_WITH_TFLITE_LIB) || defined(OS_WIN)
// TODO(crbug/1227996): Enable the model validation tests on Windows when model
// override CLI flag is supported. Currently it is not supported since the ':'
// used as model override flag delimiter, denotes the Windows drive.
#define MAYBE_TestValidAndInvalidModel DISABLED_TestValidAndInvalidModel
#else
#define MAYBE_TestValidAndInvalidModel TestValidAndInvalidModel
#endif

IN_PROC_BROWSER_TEST_P(ModelValidatorBrowserTest,
                       MAYBE_TestValidAndInvalidModel) {
  base::HistogramTester histogram_tester;
  auto model_validation_name =
      optimization_guide::GetStringNameForOptimizationTarget(
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_MODEL_VALIDATION);
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.ModelExecutor.ModelLoadingResult." +
          model_validation_name,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadingResult." +
          model_validation_name,
      is_model_valid()
          ? optimization_guide::ModelExecutorLoadingState::
                kModelFileValidAndMemoryMapped
          : optimization_guide::ModelExecutorLoadingState::kModelFileInvalid,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ModelLoadingDuration." +
          model_validation_name,
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ModelValidatorBrowserTest,
                         /* is_model_valid */ ::testing::Bool());

}  // namespace
