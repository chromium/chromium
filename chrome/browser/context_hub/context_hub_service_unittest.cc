// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <optional>
#include <tuple>

#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/auto_todos/in_memory_auto_todos_store.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/noop_memory_bank.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"
#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
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
using ::testing::ElementsAre;
using ::testing::FieldsAre;

class ContextHubServiceTest : public testing::Test {
 public:
  ContextHubServiceTest()
      : service_(&mock_personal_context_service_,
                 &mock_remote_model_executor_,
                 std::make_unique<InMemoryMemoryBank>(),
                 std::make_unique<InMemoryTabGroupStore>(),
                 /*context_hub_backend=*/nullptr,
                 std::make_unique<InMemoryAutoTodosStore>()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            browser::context_hub::mojom::kAutoTodos,
            browser::context_hub::mojom::kAutoTabGroups,
        },
        /*disabled_features=*/{});
  }
  ~ContextHubServiceTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  personal_context::MockPersonalContextService mock_personal_context_service_;
  optimization_guide::MockRemoteModelExecutor mock_remote_model_executor_;
  ContextHubService service_;
};

TEST_F(ContextHubServiceTest, GenerateAutoTodos_ServiceSuccess) {
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

TEST_F(ContextHubServiceTest, GenerateAutoTodos_ServiceError) {
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

TEST_F(ContextHubServiceTest, GenerateAutoTodos_ParseError) {
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
  service_.DeleteEntries(ids_to_delete, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future2;
  service_.GetAllEntries(get_entries_future2.GetCallback());
  EXPECT_TRUE(get_entries_future2.Get().empty());
}

TEST_F(ContextHubServiceTest, GetEntriesByIds) {
  service_.SaveTab(GURL("https://example1.com"), "Title1", "Page text 1",
                   base::DoNothing());
  service_.SaveTab(GURL("https://example2.com"), "Title2", "Page text 2",
                   base::DoNothing());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_entries_future;
  service_.GetAllEntries(get_entries_future.GetCallback());
  auto entries = get_entries_future.Get();
  ASSERT_EQ(2u, entries.size());

  base::test::TestFuture<std::vector<MemoryBankEntry>> by_ids_future;
  service_.GetEntriesByIds({entries[0].id}, by_ids_future.GetCallback());
  auto selected_entries = by_ids_future.Get();
  ASSERT_EQ(1u, selected_entries.size());
  EXPECT_EQ(entries[0].id, selected_entries[0].id);
  EXPECT_EQ(entries[0].tab_title, selected_entries[0].tab_title);
}

TEST_F(ContextHubServiceTest, GroupTabs_NoTabs) {
  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>>
      future;
  service_.GroupTabs(
      {}, "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>>());
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
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Group 1");
        group1->add_tab_ids(1);
        group1->add_tab_ids(2);

        optimization_guide::proto::TabGroupMinimal* group2 =
            group_response->add_minimal_tab_groups();
        group2->set_label("Group 2");
        group2->add_tab_ids(3);
        group2->add_tab_ids(4);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>>
      future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>>());
  std::tuple<std::vector<TabGroupEntry>, std::vector<TabData>> result =
      future.Take();
  std::vector<TabGroupEntry> groups = std::move(std::get<0>(result));
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

  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future;
  service_.GetTabGroups(
      stored_groups_future.GetCallback());
  EXPECT_THAT(
      stored_groups_future.Get(),
      ElementsAre(
          FieldsAre("group_1", "Group 1", ElementsAre(1, 2), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time())),
          FieldsAre("group_2", "Group 2", ElementsAre(3, 4), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time()))));
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

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>>
      future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>>());
  std::tuple<std::vector<TabGroupEntry>, std::vector<TabData>> result =
      future.Take();
  std::vector<TabGroupEntry> groups = std::move(std::get<0>(result));
  std::vector<TabData> ungrouped_tabs = std::move(std::get<1>(result));

  EXPECT_TRUE(groups.empty());
  ASSERT_EQ(ungrouped_tabs.size(), 2u);
  EXPECT_EQ(ungrouped_tabs[0].id, 1);
  EXPECT_EQ(ungrouped_tabs[1].id, 2);
}

TEST_F(ContextHubServiceTest, AddAndGetTabGroupChatHistory) {
  service_.AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "User message");
  task_environment_.FastForwardBy(base::Milliseconds(1));
  service_.AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT,
      "Assistant reply");

  auto history = service_.GetTabGroupChatHistory();
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0].role(),
            optimization_guide::proto::ChatHistoryTurn::ROLE_USER);
  EXPECT_EQ(history[0].message_content(), "User message");
  EXPECT_EQ(history[1].role(),
            optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
  EXPECT_EQ(history[1].message_content(), "Assistant reply");
}

TEST_F(ContextHubServiceTest, ChatHistory_LRUEviction) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      browser::context_hub::mojom::kAutoTabGroups,
      {{features::kMaxTabGroupChatHistoryTurns.name, "3"}});
  ContextHubService service(&mock_personal_context_service_,
                            &mock_remote_model_executor_,
                            std::make_unique<InMemoryMemoryBank>(),
                            std::make_unique<InMemoryTabGroupStore>(),
                            /*context_hub_backend=*/nullptr,
                            std::make_unique<InMemoryAutoTodosStore>());

  for (size_t i = 0; i < 4; ++i) {
    service.AddTabGroupChatHistoryTurn(
        optimization_guide::proto::ChatHistoryTurn::ROLE_USER,
        "Message " + base::NumberToString(i));
    task_environment_.FastForwardBy(base::Milliseconds(1));
  }

  auto history = service.GetTabGroupChatHistory();
  ASSERT_EQ(history.size(), 3u);
  // Oldest turn (Message 0) should be evicted.
  EXPECT_EQ(history.front().message_content(), "Message 1");
  EXPECT_EQ(history.back().message_content(), "Message 3");
}

TEST_F(ContextHubServiceTest, ChatHistory_Clear) {
  service_.AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "Message");
  EXPECT_EQ(service_.GetTabGroupChatHistory().size(), 1u);

  service_.ClearTabGroupChatHistory();
  EXPECT_TRUE(service_.GetTabGroupChatHistory().empty());
}

TEST_F(ContextHubServiceTest, DeleteAllTabGroups) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")},
      {2, "Tab 2", GURL("https://example2.com")}};

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
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Group 1");
        group1->add_tab_ids(1);
        group1->add_tab_ids(2);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<TabGroupEntry>, std::vector<TabData>>
      future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      future.GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>>());
  EXPECT_TRUE(future.Wait());

  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future;
  service_.GetTabGroups(stored_groups_future.GetCallback());
  EXPECT_FALSE(stored_groups_future.Get().empty());

  base::test::TestFuture<void> delete_future;
  service_.DeleteAllTabGroups(delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future2;
  service_.GetTabGroups(stored_groups_future2.GetCallback());
  EXPECT_TRUE(stored_groups_future2.Get().empty());
}

TEST_F(ContextHubServiceTest, ClearTodoFeedbacks) {
  auto feedback = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback->todo_id = "todo_1";
  feedback->liked = true;

  // Add a feedback item.
  service_.SetTodoFeedback(std::move(feedback));
  EXPECT_EQ(1u, service_.GetTodoFeedbacks().size());

  // Clear all feedback items and verify that they are cleared.
  service_.ClearTodoFeedbacks();
  EXPECT_TRUE(service_.GetTodoFeedbacks().empty());
}

TEST_F(ContextHubServiceTest, DeleteTodoFeedback) {
  auto feedback1 = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback1->todo_id = "todo_1";
  feedback1->liked = true;

  auto feedback2 = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback2->todo_id = "todo_2";
  feedback2->liked = false;

  // Add two feedback items.
  service_.SetTodoFeedback(std::move(feedback1));
  service_.SetTodoFeedback(std::move(feedback2));
  EXPECT_EQ(2u, service_.GetTodoFeedbacks().size());

  // Clear one feedback item and verify that it is cleared.
  service_.DeleteTodoFeedback("todo_1");

  auto feedbacks = service_.GetTodoFeedbacks();
  ASSERT_EQ(1u, feedbacks.size());
  EXPECT_EQ("todo_2", feedbacks[0]->todo_id);
  EXPECT_FALSE(feedbacks[0]->liked);
}

TEST_F(ContextHubServiceTest, ExecuteMemoryBankChat_Success) {
  base::test::TestFuture<void> save_tab_future1;
  service_.SaveTab(GURL("https://example.com/1"), "Title 1", "Page text 1",
                   save_tab_future1.GetCallback());
  ASSERT_TRUE(save_tab_future1.Wait());

  base::test::TestFuture<void> save_tab_future2;
  service_.SaveTextSelection(GURL("https://example.com/2"), "Title 2",
                             "Some selected text",
                             save_tab_future2.GetCallback());
  ASSERT_TRUE(save_tab_future2.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> entries_future;
  service_.GetAllEntries(entries_future.GetCallback());
  auto entries = entries_future.Take();
  ASSERT_EQ(entries.size(), 2u);

  std::vector<int64_t> ids = {entries[0].id, entries[1].id};

  EXPECT_CALL(
      mock_remote_model_executor_,
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        const auto& request =
            static_cast<const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
        EXPECT_EQ(request.request_type(),
                  optimization_guide::proto::
                      CONTEXT_HUB_REQUEST_TYPE_MEMORY_BANK_CHAT);
        EXPECT_EQ(request.user_command(), "summarize these");
        ASSERT_EQ(request.entry_items_size(), 2);

        EXPECT_TRUE(request.entry_items(0).has_memory_bank_entry());
        EXPECT_TRUE(request.entry_items(1).has_memory_bank_entry());

        optimization_guide::proto::ContextHubResponse response;
        response.mutable_memory_bank_chat_response()->set_text_response(
            "This is the LLM summary.");

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::optional<std::string>> future;
  service_.ExecuteMemoryBankChat(ids, "summarize these", future.GetCallback());
  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "This is the LLM summary.");
}

TEST_F(ContextHubServiceTest, ExecuteMemoryBankChat_Error) {
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

  base::test::TestFuture<std::optional<std::string>> future;
  std::vector<int64_t> ids = {100};
  service_.ExecuteMemoryBankChat(ids, "hello", future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace
}  // namespace context_hub
