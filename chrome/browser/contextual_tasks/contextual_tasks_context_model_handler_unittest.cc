// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_model_handler.h"

#include "base/files/file_util.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/tab_relevance_model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

namespace {

class ContextualTasksModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  ContextualTasksModelProvider() {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    model_file_path_ = test_data_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("contextual_tasks")
                           .AppendASCII("unit_test_tab_relevance.tflite");
  }

  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& any,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_TAB_RELEVANCE) {
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(model_file_path_)
                                .SetModelMetadata(model_metadata_)
                                .Build();
      observer->OnModelUpdated(optimization_target, *model_metadata);
      model_observers_.AddObserver(observer);
    }
  }

  const base::FilePath& model_file_path() const { return model_file_path_; }

  void SetModelMetadata(const optimization_guide::proto::Any& model_metadata) {
    model_metadata_ = model_metadata;
    auto model_info = optimization_guide::TestModelInfoBuilder()
                          .SetModelFilePath(model_file_path_)
                          .SetModelMetadata(model_metadata_)
                          .Build();
    model_observers_.Notify(
        &optimization_guide::OptimizationTargetModelObserver::OnModelUpdated,
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_TAB_RELEVANCE,
        *model_info);
  }

 private:
  base::ObserverList<optimization_guide::OptimizationTargetModelObserver>
      model_observers_;
  base::FilePath model_file_path_;
  optimization_guide::proto::Any model_metadata_;
};

}  // namespace

class ContextualTasksContextModelHandlerTest : public testing::Test {
 public:
  ContextualTasksContextModelHandlerTest() = default;
  ~ContextualTasksContextModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<ContextualTasksModelProvider>();
    model_handler_ = std::make_unique<ContextualTasksContextModelHandler>(
        model_provider_.get(), task_environment_.GetMainThreadTaskRunner());
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(base::PathExists(model_provider_->model_file_path()));
  }

  void SetModelMetadata(
      const optimization_guide::proto::TabRelevanceModelMetadata& metadata) {
    optimization_guide::proto::Any any;
    any.set_type_url("type.googleapis.com/TabRelevanceModelMetadata");
    metadata.SerializeToString(any.mutable_value());
    model_provider_->SetModelMetadata(any);
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();
    // Wait for any DeleteSoon tasks to run.
    task_environment_.RunUntilIdle();
  }

  ContextualTasksContextModelHandler* model_handler() const {
    return model_handler_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ContextualTasksModelProvider> model_provider_;
  std::unique_ptr<ContextualTasksContextModelHandler> model_handler_;
};

TEST_F(ContextualTasksContextModelHandlerTest, ExtractModelFeatures) {
  // Set up metadata with multiple features.
  // Sequence: Lexical (1), Length (1), Active Tab Similarity (1 title + 2
  // passages) Total features = 5.
  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_features(5);
  metadata.set_num_passages_per_tab(2);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_TITLE_LEXICAL_SIMILARITY);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_LENGTH);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_ACTIVE_TAB_SIMILARITY);

  // Set up signals with dummy values.
  QueryStateSignals query_signals;
  query_signals.query_word_count = 5.0f;
  query_signals.query_active_tab_title_similarity = 0.8f;
  query_signals.query_active_tab_passage_similarities.push_back(
      ScoredPassage{0.6f, "passage 1"});
  // Only 1 passage provided, but metadata expects 2. Second should be padded.

  TabSignals tab_signals;
  tab_signals.num_query_title_matching_words = 3.0f;

  std::vector<float> features =
      ContextualTasksContextModelHandler::ExtractModelFeatures(
          metadata, query_signals, tab_signals);

  ASSERT_EQ(features.size(), 5u);
  // Feature 0: Lexical Similarity
  EXPECT_EQ(features[0], 3.0f);
  // Feature 1: Query Length
  EXPECT_EQ(features[1], 5.0f);
  // Feature 2: Active Tab Title Similarity
  EXPECT_EQ(features[2], 0.8f);
  // Feature 3: Active Tab Passage 1 Similarity
  EXPECT_EQ(features[3], 0.6f);
  // Feature 4: Active Tab Passage 2 Similarity (Padded)
  EXPECT_EQ(features[4], 0.0f);
}

TEST_F(ContextualTasksContextModelHandlerTest, BatchExecuteModelWithSignals) {
  ContextualTasksContextModelHandler* handler = model_handler();

  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_features(25);
  metadata.add_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_LENGTH);
  for (int i = 0; i < 24; ++i) {
    metadata.add_feature_sequence(
        optimization_guide::proto::TabRelevanceModelMetadata::
            TAB_RELEVANCE_FEATURE_UNKNOWN);
  }
  SetModelMetadata(metadata);

  QueryStateSignals query_signals;
  query_signals.query_word_count = 5.0f;
  std::vector<TabSignals> batch_tab_signals(2);

  base::test::TestFuture<const std::vector<std::optional<float>>&> future;
  handler->BatchExecuteModelWithSignals(query_signals, batch_tab_signals,
                                        future.GetCallback());

  const auto& results = future.Get();
  ASSERT_EQ(results.size(), 2u);
  EXPECT_NEAR(*results[0], 0.5, 1e-1);
  EXPECT_NEAR(*results[1], 0.5, 1e-1);
}

TEST_F(ContextualTasksContextModelHandlerTest,
       BatchExecuteModelWithNoMetadata) {
  ContextualTasksContextModelHandler* handler = model_handler();

  QueryStateSignals query_signals;
  std::vector<TabSignals> batch_tab_signals(1);

  base::test::TestFuture<const std::vector<std::optional<float>>&> future;
  handler->BatchExecuteModelWithSignals(query_signals, batch_tab_signals,
                                        future.GetCallback());

  const auto& results = future.Get();
  ASSERT_EQ(results.size(), 1u);
  EXPECT_FALSE(results[0].has_value());
}

}  // namespace contextual_tasks
