// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <optional>
#include <tuple>

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/noop_memory_bank.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

class ContextHubServiceTest : public testing::Test {
 public:
  ContextHubServiceTest()
      : service_(&mock_personal_context_service_,
                 &mock_remote_model_executor_,
                 std::make_unique<InMemoryMemoryBank>()) {}
  ~ContextHubServiceTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  personal_context::MockPersonalContextService mock_personal_context_service_;
  optimization_guide::MockRemoteModelExecutor mock_remote_model_executor_;
  ContextHubService service_;
};

TEST_F(ContextHubServiceTest, GenerateAutoTodos_FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kAutoTodos);

  EXPECT_CALL(mock_personal_context_service_, FetchContext).Times(0);

  base::test::TestFuture<
      std::optional<personal_context::proto::AutoTodosResponse>>
      future;
  service_.GenerateAutoTodos(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextHubServiceTest, GenerateAutoTodos_FeatureEnabled_ServiceSuccess) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutoTodos);

  personal_context::proto::AutoTodosResponse expected_response;
  auto* todo = expected_response.add_todos();
  todo->set_title("Test Todo");
  todo->set_description("Test Description");

  personal_context::proto::Any any_response;
  expected_response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      std::optional<personal_context::proto::AutoTodosResponse>>
      future;
  service_.GenerateAutoTodos(future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value().todos_size(), 1);
  EXPECT_EQ(result.value().todos(0).title(), "Test Todo");
  EXPECT_EQ(result.value().todos(0).description(), "Test Description");
}

TEST_F(ContextHubServiceTest, GenerateAutoTodos_FeatureEnabled_ServiceError) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutoTodos);

  personal_context::ContextMemoryError expected_error =
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kGenericFailure);

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::unexpected(expected_error))));

  base::test::TestFuture<
      std::optional<personal_context::proto::AutoTodosResponse>>
      future;
  service_.GenerateAutoTodos(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextHubServiceTest, GenerateAutoTodos_FeatureEnabled_ParseError) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutoTodos);

  personal_context::proto::Any any_response;
  any_response.set_value("corrupted proto data");

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      std::optional<personal_context::proto::AutoTodosResponse>>
      future;
  service_.GenerateAutoTodos(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(ContextHubServiceTest, SaveTab) {
  base::test::TestFuture<void> save_tab_future;
  service_.SaveTab(GURL("https://example.com"), "Title", "Page text",
                   save_tab_future.GetCallback());
  EXPECT_TRUE(save_tab_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("Title", entries[0].tab_title);
  EXPECT_EQ(GURL("https://example.com"), entries[0].url);
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
}

TEST_F(ContextHubServiceTest, SaveTextSelection) {
  base::test::TestFuture<void> save_selection_future;
  service_.SaveTextSelection(GURL("https://example.com"), "Title", "Selection",
                             save_selection_future.GetCallback());
  EXPECT_TRUE(save_selection_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(MemoryBankType::kTextSelection, entries[0].type);
  EXPECT_EQ("Selection", entries[0].selected_text);
}

TEST_F(ContextHubServiceTest, DeleteEntries) {
  service_.SaveTab(GURL("https://example1.com"), "Title1", "Page text 1",
                   base::DoNothing());
  service_.SaveTab(GURL("https://example2.com"), "Title2", "Page text 2",
                   base::DoNothing());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(2u, entries.size());

  base::test::TestFuture<void> delete_future;
  std::vector<int64_t> ids_to_delete = {entries[0].id, entries[1].id};
  service_.DeleteEntries(ids_to_delete,
                         delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future2;
  service_.GetAllEntries(get_entries_future2.GetCallback());
  EXPECT_TRUE(get_entries_future2.Get().empty());
}

TEST_F(ContextHubServiceTest, GroupTabs_NoTabs) {
  base::test::TestFuture<std::vector<TabGroupData>, std::vector<TabData>>
      future;
  service_.GroupTabs(
      {},
      future.GetCallback<std::vector<TabGroupData>, std::vector<TabData>>());
  auto [groups, ungrouped_tabs] = future.Take();
  EXPECT_TRUE(groups.empty());
  EXPECT_TRUE(ungrouped_tabs.empty());
}

TEST_F(ContextHubServiceTest, GroupTabs_WithTabs) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")},
      {2, "Tab 2", GURL("https://example2.com")},
      {3, "Tab 3", GURL("https://example3.com")},
      {4, "Tab 4", GURL("https://example4.com")},
      {5, "Tab 5", GURL("https://example5.com")}};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        optimization_guide::proto::ContextHubResponse response;
        optimization_guide::proto::GroupResponse* group_response =
            response.mutable_group_response();
        optimization_guide::proto::TabGroup* group1 =
            group_response->add_tab_groups();
        group1->set_label("Group 1");
        optimization_guide::proto::Tab* tab1 = group1->add_tabs();
        tab1->set_tab_id(1);
        optimization_guide::proto::Tab* tab2 = group1->add_tabs();
        tab2->set_tab_id(2);

        optimization_guide::proto::TabGroup* group2 =
            group_response->add_tab_groups();
        group2->set_label("Group 2");
        optimization_guide::proto::Tab* tab3 = group2->add_tabs();
        tab3->set_tab_id(3);
        optimization_guide::proto::Tab* tab4 = group2->add_tabs();
        tab4->set_tab_id(4);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<TabGroupData>, std::vector<TabData>>
      future;
  service_.GroupTabs(
      std::move(input_tabs),
      future.GetCallback<std::vector<TabGroupData>, std::vector<TabData>>());
  std::tuple<std::vector<TabGroupData>, std::vector<TabData>> result =
      future.Take();
  std::vector<TabGroupData> groups = std::move(std::get<0>(result));
  std::vector<TabData> ungrouped_tabs = std::move(std::get<1>(result));

  ASSERT_EQ(groups.size(), 2u);
  EXPECT_EQ(groups[0].label, "Group 1");
  ASSERT_EQ(groups[0].tabs.size(), 2u);
  EXPECT_EQ(groups[0].tabs[0].id, 1);
  EXPECT_EQ(groups[0].tabs[1].id, 2);

  EXPECT_EQ(groups[1].label, "Group 2");
  ASSERT_EQ(groups[1].tabs.size(), 2u);
  EXPECT_EQ(groups[1].tabs[0].id, 3);
  EXPECT_EQ(groups[1].tabs[1].id, 4);

  ASSERT_EQ(ungrouped_tabs.size(), 1u);
  EXPECT_EQ(ungrouped_tabs[0].id, 5);
}

TEST_F(ContextHubServiceTest, GroupTabs_MESError) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")},
      {2, "Tab 2", GURL("https://example2.com")}};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(),
                nullptr);
          });

  base::test::TestFuture<std::vector<TabGroupData>, std::vector<TabData>>
      future;
  service_.GroupTabs(
      std::move(input_tabs),
      future.GetCallback<std::vector<TabGroupData>, std::vector<TabData>>());
  std::tuple<std::vector<TabGroupData>, std::vector<TabData>> result =
      future.Take();
  std::vector<TabGroupData> groups = std::move(std::get<0>(result));
  std::vector<TabData> ungrouped_tabs = std::move(std::get<1>(result));

  EXPECT_TRUE(groups.empty());
  ASSERT_EQ(ungrouped_tabs.size(), 2u);
  EXPECT_EQ(ungrouped_tabs[0].id, 1);
  EXPECT_EQ(ungrouped_tabs[1].id, 2);
}

}  // namespace
}  // namespace context_hub
