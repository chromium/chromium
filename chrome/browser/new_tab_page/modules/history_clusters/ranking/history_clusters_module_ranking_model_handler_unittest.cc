// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_handler.h"

#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_metadata.pb.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ElementsAre;

class FakeModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const absl::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    CHECK_EQ(
        optimization_target,
        optimization_guide::proto::
            OPTIMIZATION_TARGET_NEW_TAB_PAGE_HISTORY_CLUSTERS_MODULE_RANKING);
    was_registered_ = true;
  }
  bool was_registered() const { return was_registered_; }

 private:
  bool was_registered_ = false;
};

class HistoryClustersModuleRankingModelHandlerTest : public testing::Test {
 public:
  HistoryClustersModuleRankingModelHandlerTest() {
    model_provider_ = std::make_unique<FakeModelProvider>();
    model_handler_ = std::make_unique<HistoryClustersModuleRankingModelHandler>(
        model_provider_.get());
    EXPECT_TRUE(model_provider_->was_registered());

    // Just use the omnibox fake model since it just adds two floats which
    // is all we need right now.
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("omnibox")
                           .AppendASCII("adder.tflite");
  }
  ~HistoryClustersModuleRankingModelHandlerTest() override {
    // Make sure everything is deleted on the bg thread the underlying executor
    // is on.
    model_handler_.reset();
    task_environment_.RunUntilIdle();
  }

  HistoryClustersModuleRankingModelHandler* model_handler() {
    return model_handler_.get();
  }

  void PushModelFileToModelExecutor(
      absl::optional<
          new_tab_page::proto::HistoryClustersModuleRankingModelMetadata>
          metadata) {
    absl::optional<optimization_guide::proto::Any> any;

    // Craft a correct Any proto in the case we passed in metadata.
    if (metadata.has_value()) {
      std::string serialized_metadata;
      (*metadata).SerializeToString(&serialized_metadata);
      optimization_guide::proto::Any any_proto;
      any = absl::make_optional(any_proto);
      any->set_value(serialized_metadata);
      any->set_type_url(
          "type.googleapis.com/"
          "new_tab_page.protos.HistoryClustersModuleRankingModelMetadata");
    }

    auto model_metadata = optimization_guide::TestModelInfoBuilder()
                              .SetModelMetadata(any)
                              .SetModelFilePath(model_file_path_)
                              .SetVersion(123)
                              .Build();
    model_handler()->OnModelUpdated(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_NEW_TAB_PAGE_HISTORY_CLUSTERS_MODULE_RANKING,
        *model_metadata);
    task_environment_.RunUntilIdle();
  }

  std::vector<float> GetOutputs(
      std::vector<HistoryClustersModuleRankingSignals> inputs) {
    std::vector<float> outputs;

    base::RunLoop run_loop;
    model_handler()->ExecuteBatch(
        &inputs,
        base::BindOnce(
            [](base::RunLoop* run_loop, std::vector<float>* out_outputs,
               std::vector<float> outputs) {
              *out_outputs = std::move(outputs);
              run_loop->Quit();
            },
            &run_loop, &outputs));

    run_loop.Run();

    return outputs;
  }

 private:
  std::unique_ptr<FakeModelProvider> model_provider_;
  std::unique_ptr<HistoryClustersModuleRankingModelHandler> model_handler_;

  base::FilePath model_file_path_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(HistoryClustersModuleRankingModelHandlerTest, ModelNotAvailable) {
  EXPECT_FALSE(model_handler()->CanExecuteAvailableModel());
}

TEST_F(HistoryClustersModuleRankingModelHandlerTest, ModelUpdatedBadMetadata) {
  PushModelFileToModelExecutor(/*metadata=*/absl::nullopt);

  EXPECT_FALSE(model_handler()->CanExecuteAvailableModel());

  HistoryClustersModuleRankingSignals inputs1;
  inputs1.duration_since_most_recent_visit = base::Minutes(2);
  inputs1.belongs_to_boosted_category = false;

  HistoryClustersModuleRankingSignals inputs2;
  inputs2.duration_since_most_recent_visit = base::Minutes(5);
  inputs2.belongs_to_boosted_category = true;

  // Should return 0 if tried to execute model.
  EXPECT_THAT(GetOutputs({inputs1, inputs2}), ElementsAre(0, 0));
}

TEST_F(HistoryClustersModuleRankingModelHandlerTest,
       ModelUpdatedVersionTooHigh) {
  new_tab_page::proto::HistoryClustersModuleRankingModelMetadata metadata;
  metadata.set_version(HistoryClustersModuleRankingSignals::kClientVersion + 1);
  PushModelFileToModelExecutor(metadata);

  EXPECT_FALSE(model_handler()->CanExecuteAvailableModel());
}

TEST_F(HistoryClustersModuleRankingModelHandlerTest,
       ModelUpdatedVersionCorrectVersion) {
  new_tab_page::proto::HistoryClustersModuleRankingModelMetadata metadata;
  metadata.set_version(HistoryClustersModuleRankingSignals::kClientVersion);
  PushModelFileToModelExecutor(metadata);

  EXPECT_TRUE(model_handler()->CanExecuteAvailableModel());
}

TEST_F(HistoryClustersModuleRankingModelHandlerTest,
       ModelExecutedMultipleInputs) {
  HistoryClustersModuleRankingSignals inputs1;
  inputs1.duration_since_most_recent_visit = base::Minutes(2);
  inputs1.belongs_to_boosted_category = false;
  inputs1.num_visits_with_image = 3;
  inputs1.num_total_visits = 4;
  inputs1.num_unique_hosts = 2;
  inputs1.num_abandoned_carts = 1;

  HistoryClustersModuleRankingSignals inputs2;
  inputs2.duration_since_most_recent_visit = base::Minutes(5);
  inputs2.belongs_to_boosted_category = true;
  inputs2.num_visits_with_image = 2;
  inputs2.num_total_visits = 10;
  inputs2.num_unique_hosts = 3;
  inputs2.num_abandoned_carts = 0;

  {
    new_tab_page::proto::HistoryClustersModuleRankingModelMetadata metadata;
    metadata.set_version(HistoryClustersModuleRankingSignals::kClientVersion);
    metadata.add_signals(
        new_tab_page::proto::
            HISTORY_CLUSTERS_MODULE_RANKING_MINUTES_SINCE_MOST_RECENT_VISIT);
    metadata.add_signals(
        new_tab_page::proto::
            HISTORY_CLUSTERS_MODULE_RANKING_BELONGS_TO_BOOSTED_CATEGORY);
    PushModelFileToModelExecutor(metadata);

    EXPECT_TRUE(model_handler()->CanExecuteAvailableModel());

    EXPECT_THAT(GetOutputs({inputs1, inputs2}), ElementsAre(2 + 0, 5 + 1));
  }

  {
    new_tab_page::proto::HistoryClustersModuleRankingModelMetadata metadata;
    metadata.set_version(HistoryClustersModuleRankingSignals::kClientVersion);
    metadata.add_signals(
        new_tab_page::proto::
            HISTORY_CLUSTERS_MODULE_RANKING_NUM_VISITS_WITH_IMAGE);
    metadata.add_signals(
        new_tab_page::proto::HISTORY_CLUSTERS_MODULE_RANKING_NUM_TOTAL_VISITS);
    PushModelFileToModelExecutor(metadata);

    EXPECT_TRUE(model_handler()->CanExecuteAvailableModel());

    EXPECT_THAT(GetOutputs({inputs1, inputs2}), ElementsAre(3 + 4, 2 + 10));
  }

  {
    new_tab_page::proto::HistoryClustersModuleRankingModelMetadata metadata;
    metadata.set_version(HistoryClustersModuleRankingSignals::kClientVersion);
    metadata.add_signals(
        new_tab_page::proto::HISTORY_CLUSTERS_MODULE_RANKING_NUM_UNIQUE_HOSTS);
    metadata.add_signals(
        new_tab_page::proto::
            HISTORY_CLUSTERS_MODULE_RANKING_NUM_ABANDONED_CARTS);
    PushModelFileToModelExecutor(metadata);

    EXPECT_TRUE(model_handler()->CanExecuteAvailableModel());

    EXPECT_THAT(GetOutputs({inputs1, inputs2}), ElementsAre(2 + 1, 3 + 0));
  }
}

}  // namespace
