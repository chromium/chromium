// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/visual_search_classifier_host.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/companion/visual_search/features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion::visual_search {

namespace {

constexpr char kValidUrl[] = "https://foo.com/";

base::FilePath model_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("companion_visual_search")
      .AppendASCII("test-model-quantized.tflite");
}

}  // namespace

class VisualSearchClassifierHostTest : public ChromeRenderViewHostTestHarness {
 public:
  VisualSearchClassifierHostTest() : url_(kValidUrl) {}
  ~VisualSearchClassifierHostTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    test_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    service_ = std::make_unique<
        companion::visual_search::VisualSearchSuggestionsService>(
        test_model_provider_.get(), background_task_runner);

    visual_search_host_ =
        std::make_unique<companion::visual_search::VisualSearchClassifierHost>(
            service_.get());
  }

  void SetModelPath() {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);

    base::flat_set<base::FilePath> additional_files;
    additional_files.insert(model_file_path());

    model_info_ = optimization_guide::TestModelInfoBuilder()
                      .SetModelFilePath(model_file_path())
                      .SetAdditionalFiles(additional_files)
                      .SetVersion(123)
                      .Build();

    service_->OnModelUpdated(
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
        *model_info_);
  }

  void TearDown() override {
    service_->Shutdown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      test_model_provider_;
  std::unique_ptr<optimization_guide::ModelInfo> model_info_;
  std::unique_ptr<companion::visual_search::VisualSearchSuggestionsService>
      service_;
  std::unique_ptr<companion::visual_search::VisualSearchClassifierHost>
      visual_search_host_;
  const GURL url_;
  base::HistogramTester histogram_tester_;
};

TEST_F(VisualSearchClassifierHostTest, StartClassification) {
  SetModelPath();
  VisualSearchClassifierHost::ResultCallback callback =
      base::BindOnce([](std::vector<std::string> results) {});
  visual_search_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount("Companion.VisualSearch.ModelFileSuccess",
                                      true, 1);
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualSearch.StartClassificationSuccess", true, 1);
}

TEST_F(VisualSearchClassifierHostTest, StartClassification_WithOverride) {
  SetModelPath();
  const std::string config_string = "config_string";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kVisualSearchConfigForCompanion, config_string);
  VisualSearchClassifierHost::ResultCallback callback =
      base::BindOnce([](std::vector<std::string> results) {});
  visual_search_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount("Companion.VisualSearch.ModelFileSuccess",
                                      true, 1);
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualSearch.StartClassificationSuccess", true, 1);
}

TEST_F(VisualSearchClassifierHostTest, StartClassification_NoModelSet) {
  VisualSearchClassifierHost::ResultCallback callback =
      base::BindOnce([](std::vector<std::string> results) {});
  visual_search_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  base::RunLoop().RunUntilIdle();

  // ModelFileSuccess is never called because the |OnModelUpdate| is never
  // called by the |service_| since we never setup the model path.
  histogram_tester_.ExpectTotalCount("Companion.VisualSearch.ModelFileSuccess",
                                     0);
}

TEST_F(VisualSearchClassifierHostTest, StartClassification_WithCancellation) {
  SetModelPath();
  VisualSearchClassifierHost::ResultCallback callback =
      base::BindOnce([](std::vector<std::string> results) {});
  visual_search_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  visual_search_host_->CancelClassification();
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualSearch.ClassificationCancelled", true, 1);
  histogram_tester_.ExpectBucketCount("Companion.VisualSearch.ModelFileSuccess",
                                      true, 1);
  histogram_tester_.ExpectBucketCount("Companion.VisualSearch.MismatchURL",
                                      true, 1);
}

}  // namespace companion::visual_search
