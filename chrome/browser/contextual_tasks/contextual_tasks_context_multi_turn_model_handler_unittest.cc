// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_multi_turn_model_handler.h"

#include "base/files/file_util.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
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

class ContextualTasksMultiTurnModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  ContextualTasksMultiTurnModelProvider() {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    model_file_path_ = test_data_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("contextual_tasks")
                           .AppendASCII("multi_turn_tab_relevance.tflite");
  }

  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& any,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_MULTI_TURN_TAB_RELEVANCE) {
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
            OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_MULTI_TURN_TAB_RELEVANCE,
        *model_info);
  }

 private:
  base::ObserverList<optimization_guide::OptimizationTargetModelObserver>
      model_observers_;
  base::FilePath model_file_path_;
  optimization_guide::proto::Any model_metadata_;
};

class TestContextualTasksContextMultiTurnModelHandler
    : public ContextualTasksContextMultiTurnModelHandler {
 public:
  using ContextualTasksContextMultiTurnModelHandler::
      ContextualTasksContextMultiTurnModelHandler;

  void BatchExecuteModelWithInput(
      BatchExecutionCallback callback,
      typename optimization_guide::ModelExecutor<
          MultiTurnModelOutput,
          const MultiTurnModelInput&>::ConstRefInputVector batch_input)
      override {
    last_inputs_ = batch_input;
    ContextualTasksContextMultiTurnModelHandler::BatchExecuteModelWithInput(
        std::move(callback), batch_input);
  }

  const std::vector<MultiTurnModelInput>& last_inputs() const {
    return last_inputs_;
  }

 private:
  std::vector<MultiTurnModelInput> last_inputs_;
};

}  // namespace

class ContextualTasksContextMultiTurnModelHandlerTest : public testing::Test {
 public:
  ContextualTasksContextMultiTurnModelHandlerTest() = default;
  ~ContextualTasksContextMultiTurnModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<ContextualTasksMultiTurnModelProvider>();
    model_handler_ =
        std::make_unique<ContextualTasksContextMultiTurnModelHandler>(
            model_provider_.get(), task_environment_.GetMainThreadTaskRunner());
    ASSERT_TRUE(base::PathExists(model_provider_->model_file_path()));
  }

  void SetModelMetadata(
      const optimization_guide::proto::TabRelevanceModelMetadata& metadata) {
    optimization_guide::proto::Any any;
    any.set_type_url("type.googleapis.com/TabRelevanceModelMetadata");
    metadata.SerializeToString(any.mutable_value());
    model_provider_->SetModelMetadata(any);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return model_handler_->GetModelInfo().has_value(); }));
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();

    // Ensure all background tasks and delayed deletions are fully processed.
    task_environment_.RunUntilIdle();
  }

  ContextualTasksContextMultiTurnModelHandler* model_handler() const {
    return model_handler_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ContextualTasksMultiTurnModelProvider> model_provider_;
  std::unique_ptr<ContextualTasksContextMultiTurnModelHandler> model_handler_;
};

TEST_F(ContextualTasksContextMultiTurnModelHandlerTest,
       ExecuteModelWithSignals) {
  ContextualTasksContextMultiTurnModelHandler* handler = model_handler();

  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_conversation_thread_turns(1);
  metadata.set_max_titles_per_thread(1);
  metadata.set_num_embedding_dimensions(4);
  metadata.set_num_passages_per_tab(1);

  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CONVERSATION_THREAD_QUERIES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CONVERSATION_THREAD_TITLES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_TITLE_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_PASSAGES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_TITLE_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_PASSAGES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_LENGTH);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_TITLE_LEXICAL_SIMILARITY);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_RECENCY);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_LAST_DURATION);
  SetModelMetadata(metadata);

  QueryStateSignals query_signals;
  query_signals.query_word_count = 3;
  query_signals.query_embedding = {0.1f, 0.2f, 0.3f, 0.4f};
  query_signals.conversation_thread_queries_embeddings.push_back(
      {0.4f, 0.5f, 0.6f, 0.7f});
  query_signals.conversation_thread_titles_embeddings.push_back(
      {0.5f, 0.6f, 0.7f, 0.8f});
  query_signals.context_tab_title_embedding = {0.6f, 0.7f, 0.8f, 0.9f};
  query_signals.context_tab_passages_embeddings.push_back(
      {0.7f, 0.8f, 0.9f, 1.0f});

  std::vector<TabSignals> batch_tab_signals(1);
  batch_tab_signals[0].candidate_title_embedding = {1.0f, 1.1f, 1.2f, 1.3f};
  batch_tab_signals[0].candidate_passages_embeddings.push_back(
      {1.1f, 1.2f, 1.3f, 1.4f});
  batch_tab_signals[0].num_query_title_matching_words = 1;
  batch_tab_signals[0].duration_since_last_active = base::Seconds(1);
  batch_tab_signals[0].duration_of_last_visit = base::Seconds(2);

  base::test::TestFuture<const std::vector<std::optional<MultiTurnModelOutput>>&>
      future;
  handler->BatchExecuteModelWithSignalsForConversationThread(
      query_signals, batch_tab_signals, future.GetCallback());

  const auto& results = future.Get();
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0].has_value());

  // Verify score
  EXPECT_GE(results[0]->score, 0.0f);
  EXPECT_LE(results[0]->score, 1.0f);
  // Verify similarities shape is correct (dummy model has 5 similarities)
  EXPECT_EQ(results[0]->cosine_similarities.size(), 5u);
}

TEST_F(ContextualTasksContextMultiTurnModelHandlerTest,
       ExtractMultiTurnModelFeatures) {
  auto test_handler =
      std::make_unique<TestContextualTasksContextMultiTurnModelHandler>(
          model_provider_.get(), task_environment_.GetMainThreadTaskRunner());

  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_conversation_thread_turns(1);
  metadata.set_max_titles_per_thread(1);
  metadata.set_num_embedding_dimensions(4);
  metadata.set_num_passages_per_tab(1);

  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CONVERSATION_THREAD_QUERIES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CONVERSATION_THREAD_TITLES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_TITLE_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_PASSAGES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_TITLE_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_PASSAGES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_LENGTH);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_TITLE_LEXICAL_SIMILARITY);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_RECENCY);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_LAST_DURATION);

  optimization_guide::proto::Any any;
  any.set_type_url("type.googleapis.com/TabRelevanceModelMetadata");
  metadata.SerializeToString(any.mutable_value());
  model_provider_->SetModelMetadata(any);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_handler->GetModelInfo().has_value(); }));

  QueryStateSignals query_signals;
  query_signals.query_word_count = 3;
  query_signals.query_embedding = {0.1f, 0.2f, 0.3f, 0.4f};
  query_signals.conversation_thread_queries_embeddings.push_back(
      {0.4f, 0.5f, 0.6f, 0.7f});
  query_signals.conversation_thread_titles_embeddings.push_back(
      {0.5f, 0.6f, 0.7f, 0.8f});
  query_signals.context_tab_title_embedding = {0.6f, 0.7f, 0.8f, 0.9f};
  query_signals.context_tab_passages_embeddings.push_back(
      {0.7f, 0.8f, 0.9f, 1.0f});

  std::vector<TabSignals> batch_tab_signals(1);
  batch_tab_signals[0].candidate_title_embedding = {1.0f, 1.1f, 1.2f, 1.3f};
  batch_tab_signals[0].candidate_passages_embeddings.push_back(
      {1.1f, 1.2f, 1.3f, 1.4f});
  batch_tab_signals[0].num_query_title_matching_words = 2;
  batch_tab_signals[0].duration_since_last_active = base::Seconds(120);
  batch_tab_signals[0].duration_of_last_visit = base::Seconds(300);

  base::test::TestFuture<const std::vector<std::optional<MultiTurnModelOutput>>&>
      future;
  test_handler->BatchExecuteModelWithSignalsForConversationThread(
      query_signals, batch_tab_signals, future.GetCallback());

  // Wait for execution to complete to ensure proper cleanup.
  auto results = future.Get();
  ASSERT_EQ(results.size(), 1u);

  // Wait until we captured the inputs
  const auto& inputs = test_handler->last_inputs();
  ASSERT_EQ(inputs.size(), 1u);

  // Assert correct values and padding dimensions
  // 1. query_embedding
  EXPECT_EQ(inputs[0].query_embedding.size(), 4u);
  EXPECT_NEAR(inputs[0].query_embedding[0], 0.1f, 1e-5);
  EXPECT_NEAR(inputs[0].query_embedding[3], 0.4f, 1e-5);

  // 2. conversation_queries_embeddings
  EXPECT_EQ(inputs[0].conversation_thread_queries_embeddings.size(), 4u);
  EXPECT_NEAR(inputs[0].conversation_thread_queries_embeddings[0], 0.4f, 1e-5);

  // 3. conversation_titles_embeddings
  EXPECT_EQ(inputs[0].conversation_thread_titles_embeddings.size(), 4u);
  EXPECT_NEAR(inputs[0].conversation_thread_titles_embeddings[0], 0.5f, 1e-5);

  // 4. active_title_embedding
  EXPECT_EQ(inputs[0].active_title_embedding.size(), 4u);
  EXPECT_NEAR(inputs[0].active_title_embedding[0], 0.6f, 1e-5);

  // 5. active_passages_embeddings
  EXPECT_EQ(inputs[0].active_passages_embeddings.size(), 4u);
  EXPECT_NEAR(inputs[0].active_passages_embeddings[0], 0.7f, 1e-5);

  // 6. candidate_title_embedding
  EXPECT_EQ(inputs[0].candidate_tab_title_embedding.size(), 4u);
  EXPECT_NEAR(inputs[0].candidate_tab_title_embedding[0], 1.0f, 1e-5);

  // 7. candidate_passages_embeddings
  EXPECT_EQ(inputs[0].candidate_tab_passages_embeddings.size(), 4u);
  EXPECT_NEAR(inputs[0].candidate_tab_passages_embeddings[0], 1.1f, 1e-5);

  // 8. query_length
  ASSERT_EQ(inputs[0].query_length.size(), 1u);
  EXPECT_NEAR(inputs[0].query_length[0], 3.0f, 1e-5);

  // 9. lexical match (now raw match count, as per Abhay's feed!)
  ASSERT_EQ(inputs[0].query_title_lexical_similarity.size(), 1u);
  EXPECT_NEAR(inputs[0].query_title_lexical_similarity[0], 2.0f, 1e-5);

  // 10. tab recency
  ASSERT_EQ(inputs[0].candidate_tab_recency.size(), 1u);
  EXPECT_NEAR(inputs[0].candidate_tab_recency[0], 120.0f, 1e-5);

  // 11. tab last visitor
  ASSERT_EQ(inputs[0].candidate_tab_last_duration.size(), 1u);
  EXPECT_NEAR(inputs[0].candidate_tab_last_duration[0], 300.0f, 1e-5);
}

}  // namespace contextual_tasks
