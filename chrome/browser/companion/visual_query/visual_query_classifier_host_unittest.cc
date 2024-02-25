// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_query/visual_query_classifier_host.h"

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
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/common/companion/visual_query.mojom.h"
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
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion::visual_query {

namespace {

constexpr char kValidUrl[] = "https://foo.com/";

base::FilePath model_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("companion_visual_query")
      .AppendASCII("test-model-quantized.tflite");
}

base::FilePath invalid_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("invalid-path");
}

const SkBitmap create_bitmap(int width, int height, int r, int g, int b) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseARGB(255, r, g, b);
  return bitmap;
}

void WaitForHostClassification() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
     FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
  run_loop.Run();
}
}  // namespace

class VisualQueryClassifierHostTest : public ChromeRenderViewHostTestHarness {
 public:
  VisualQueryClassifierHostTest() : url_(kValidUrl) {}
  ~VisualQueryClassifierHostTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    test_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    service_ = std::make_unique<
        companion::visual_query::VisualQuerySuggestionsService>(
        test_model_provider_.get(), background_task_runner);

    visual_query_host_ =
        std::make_unique<companion::visual_query::VisualQueryClassifierHost>(
            service_.get());
  }

  void SetModelPath() {
    model_info_ = optimization_guide::TestModelInfoBuilder()
                      .SetModelFilePath(model_file_path())
                      .SetVersion(123)
                      .Build();

    service_->OnModelUpdated(
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
        *model_info_);
  }

  void SetInvalidModelPath() {
    model_info_ = optimization_guide::TestModelInfoBuilder()
                      .SetModelFilePath(invalid_file_path())
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
  std::unique_ptr<companion::visual_query::VisualQuerySuggestionsService>
      service_;
  std::unique_ptr<companion::visual_query::VisualQueryClassifierHost>
      visual_query_host_;
  const GURL url_;
  base::HistogramTester histogram_tester_;
};

TEST_F(VisualQueryClassifierHostTest, StartClassification) {
  SetModelPath();
  VisualQueryClassifierHost::ResultCallback callback =
      base::BindOnce([](const VisualSuggestionsResults results,
                        const VisualSuggestionsMetrics stats) {});
  visual_query_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  WaitForHostClassification();
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassifierModelAvailable", true, 1);
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassificationInitStatus",
      companion::visual_query::InitStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassifierInitializationLatency", 1);
  // ClassificationLatency is not recorded until HandleClassification().
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassificationLatency", 0);
}

TEST_F(VisualQueryClassifierHostTest, StartClassification_NoModelSet) {
  VisualQueryClassifierHost::ResultCallback callback =
      base::BindOnce([](const VisualSuggestionsResults results,
                        const VisualSuggestionsMetrics stats) {});
  visual_query_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  WaitForHostClassification();

  // ModelFileSuccess is never called because the |OnModelUpdate| is never
  // called by the |service_| since we never setup the model path.
  // The following calls are not made for the same reason as above.
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassificationInitStatus",
      companion::visual_query::InitStatus::kFetchModel, 1);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassifierInitializationLatency", 0);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassificationLatency", 0);
}

TEST_F(VisualQueryClassifierHostTest, StartClassification_WithInvalidModel) {
  SetInvalidModelPath();
  VisualQueryClassifierHost::ResultCallback callback =
      base::BindOnce([](const VisualSuggestionsResults results,
                        const VisualSuggestionsMetrics stats) {});
  visual_query_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  WaitForHostClassification();

  // We expect empty result right away since we don't have a good model.
  EXPECT_EQ(visual_query_host_->GetVisualResult(url_).value().size(), 0U);

  // ModelFileSuccess is never called because the |OnModelUpdate| is never
  // called because file path is not valid.
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassificationInitStatus",
      companion::visual_query::InitStatus::kFetchModel, 1);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassifierInitializationLatency", 0);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassificationLatency", 0);
}

TEST_F(VisualQueryClassifierHostTest, StartClassification_WithCancellation) {
  SetModelPath();
  VisualQueryClassifierHost::ResultCallback callback =
      base::BindOnce([](const VisualSuggestionsResults results,
                        const VisualSuggestionsMetrics stats) {});
  visual_query_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  GURL url("https://foo.bar");
  visual_query_host_->CancelClassification(url);
  WaitForHostClassification();

  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassificationInitStatus",
      companion::visual_query::InitStatus::kQueryCancelled, 1);
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassifierModelAvailable", true, 1);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassifierInitializationLatency", 0);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassificationLatency", 0);
}

TEST_F(VisualQueryClassifierHostTest, HandleClassification) {
  SetModelPath();
  VisualQueryClassifierHost::ResultCallback callback =
      base::BindOnce([](const VisualSuggestionsResults results,
                        const VisualSuggestionsMetrics stats) {
        EXPECT_EQ(results.size(), 1U);
      });
  visual_query_host_->StartClassification(
      web_contents()->GetPrimaryMainFrame(), url_, std::move(callback));
  WaitForHostClassification();

  std::vector<mojom::VisualQuerySuggestionPtr> results;
  SkBitmap result = create_bitmap(1000, 1000, 128, 128, 255);
  results.emplace_back(mojom::VisualQuerySuggestion::New(result, "alt-text"));

  mojom::ClassificationStatsPtr stats =
      mojom::ClassificationStats::New(mojom::ClassificationStats());
  visual_query_host_->HandleClassification(std::move(results),
                                            std::move(stats));
  WaitForHostClassification();

  // We expect last result to have size of 1 for given url.
  EXPECT_EQ(visual_query_host_->GetVisualResult(url_).value().size(), 1U);

  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassifierModelAvailable", true, 1);
  histogram_tester_.ExpectBucketCount(
      "Companion.VisualQuery.ClassificationInitStatus",
      companion::visual_query::InitStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassifierInitializationLatency", 1);
  histogram_tester_.ExpectTotalCount(
      "Companion.VisualQuery.ClassificationLatency", 1);
}

}  // namespace companion::visual_query
