// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/auto_todos/in_memory_auto_todos_store.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"
#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;

class MockServiceObserver : public ContextHubService::Observer {
 public:
  MockServiceObserver() = default;
  MockServiceObserver(const MockServiceObserver&) = delete;
  MockServiceObserver& operator=(const MockServiceObserver&) = delete;
  ~MockServiceObserver() override = default;

  MOCK_METHOD(void,
              OnAutoTodosChanged,
              (base::span<const AutoTodoEntry>),
              (override));
};

class ContextHubServiceTest : public testing::Test {
 public:
  ContextHubServiceTest()
      : service_(&mock_personal_context_service_,
                 &mock_remote_model_executor_,
                 &fake_tab_group_sync_service_,
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
  tab_groups::FakeTabGroupSyncService fake_tab_group_sync_service_;
  ContextHubService service_;
};

TEST_F(ContextHubServiceTest, GenerateFirstPartyAutoTodos_ServiceSuccess) {
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

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  // Initial clearing of the store.
  EXPECT_CALL(observer, OnAutoTodosChanged(IsEmpty()));
  // Notification after adding the todos.
  EXPECT_CALL(observer,
              OnAutoTodosChanged(ElementsAre(AllOf(
                  Field(&AutoTodoEntry::title, "Test Todo"),
                  Field(&AutoTodoEntry::description, "Test Description")))));

  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateFirstPartyAutoTodos_ServiceError) {
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

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnAutoTodosChanged(_)).Times(0);

  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateFirstPartyAutoTodos_ParseError) {
  personal_context::proto::Any any_response;
  any_response.set_value("corrupted proto data");

  EXPECT_CALL(
      mock_personal_context_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnAutoTodosChanged(_)).Times(0);

  base::test::TestFuture<bool> future;
  service_.GenerateFirstPartyAutoTodos(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(ContextHubServiceTest, GenerateTabBasedTodos) {
  std::vector<TabData> input_tabs = {
      {1, "Tab 1", GURL("https://example1.com")}};

  base::test::TestFuture<bool> future;
  service_.GenerateTabBasedTodos(std::move(input_tabs), future.GetCallback());

  EXPECT_FALSE(future.Get());
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
  service_.GetTabGroups(stored_groups_future.GetCallback());
  EXPECT_THAT(
      stored_groups_future.Get(),
      ElementsAre(
          FieldsAre(testing::Ne(""), "Group 1", ElementsAre(1, 2), _,
                    testing::Ne(base::Time()), testing::Ne(base::Time())),
          FieldsAre(testing::Ne(""), "Group 2", ElementsAre(3, 4), _,
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
  ContextHubService service(
      &mock_personal_context_service_, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, std::make_unique<InMemoryMemoryBank>(),
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

TEST_F(ContextHubServiceTest, UpdateAutoTodo) {
  MockServiceObserver observer;
  base::ScopedObservation<ContextHubService, ContextHubService::Observer>
      observation(&observer);
  observation.Observe(&service_);

  AutoTodoEntry entry;
  entry.id = "todo_1";
  entry.title = "Initial Title";
  entry.status = AutoTodoEntry::Status::kActive;

  EXPECT_CALL(
      observer,
      OnAutoTodosChanged(ElementsAre(AllOf(
          Field(&AutoTodoEntry::id, "todo_1"),
          Field(&AutoTodoEntry::title, "Initial Title"),
          Field(&AutoTodoEntry::status, AutoTodoEntry::Status::kActive)))));

  base::test::TestFuture<bool> add_future;
  service_.UpdateAutoTodo(entry, add_future.GetCallback());
  EXPECT_TRUE(add_future.Get());

  AutoTodoEntry updated_entry;
  updated_entry.id = "todo_1";
  updated_entry.title = "Updated Title";
  updated_entry.status = AutoTodoEntry::Status::kCompleted;

  EXPECT_CALL(
      observer,
      OnAutoTodosChanged(ElementsAre(AllOf(
          Field(&AutoTodoEntry::id, "todo_1"),
          Field(&AutoTodoEntry::title, "Updated Title"),
          Field(&AutoTodoEntry::status, AutoTodoEntry::Status::kCompleted)))));

  base::test::TestFuture<bool> update_future;
  service_.UpdateAutoTodo(updated_entry, update_future.GetCallback());
  EXPECT_TRUE(update_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].id, "todo_1");
  EXPECT_EQ(items[0].title, "Updated Title");
  EXPECT_EQ(items[0].status, AutoTodoEntry::Status::kCompleted);
}

TEST_F(ContextHubServiceTest, GetAutoTodos) {
  base::test::TestFuture<std::vector<AutoTodoEntry>> get_empty_future;
  service_.GetAutoTodos(get_empty_future.GetCallback());
  EXPECT_TRUE(get_empty_future.Get().empty());

  AutoTodoEntry entry1;
  entry1.id = "todo_1";
  entry1.title = "Todo 1";
  entry1.description = "Description 1";
  entry1.status = AutoTodoEntry::Status::kActive;
  entry1.importance_score = 0.8f;
  entry1.data = FirstPartyData{
      .source_references = {{.url = GURL("https://mail.google.com/1"),
                             .subject = "Subject 1"}},
      .actionable_url = GURL("https://example.com/1"),
  };

  AutoTodoEntry entry2;
  entry2.id = "todo_2";
  entry2.title = "Todo 2";
  entry2.description = "Description 2";
  entry2.status = AutoTodoEntry::Status::kCompleted;
  entry2.importance_score = 0.5f;

  base::test::TestFuture<bool> update_future1;
  service_.UpdateAutoTodo(entry1, update_future1.GetCallback());
  EXPECT_TRUE(update_future1.Get());

  base::test::TestFuture<bool> update_future2;
  service_.UpdateAutoTodo(entry2, update_future2.GetCallback());
  EXPECT_TRUE(update_future2.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service_.GetAutoTodos(get_future.GetCallback());
  auto items = get_future.Get();
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].id, "todo_2");
  EXPECT_EQ(items[0].title, "Todo 2");
  EXPECT_EQ(items[0].description, "Description 2");
  EXPECT_EQ(items[0].status, AutoTodoEntry::Status::kCompleted);
  EXPECT_FLOAT_EQ(items[0].importance_score, 0.5f);

  EXPECT_EQ(items[1].id, "todo_1");
  EXPECT_EQ(items[1].title, "Todo 1");
  EXPECT_EQ(items[1].description, "Description 1");
  EXPECT_EQ(items[1].status, AutoTodoEntry::Status::kActive);
  EXPECT_FLOAT_EQ(items[1].importance_score, 0.8f);
  EXPECT_TRUE(items[1].is_first_party());
}

TEST_F(ContextHubServiceTest, ConfirmAllTabGroups_EmptyGroups) {
  base::test::TestFuture<bool, std::vector<base::Uuid>> future;
  service_.ConfirmAllTabGroups(future.GetCallback());
  auto [success, added_group_guids] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_TRUE(added_group_guids.empty());
}

TEST_F(ContextHubServiceTest, GetConfirmedTabGroups) {
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  tab_groups::SavedTabGroupTab tab(GURL("https://example.com"), u"Example",
                                   group.saved_guid(), /*position=*/0);
  group.AddTabLocally(tab);
  fake_tab_group_sync_service_.AddGroup(group);

  std::vector<TabGroupEntry> groups =
      service_.GetConfirmedTabGroups();
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_EQ(groups[0].id, group.saved_guid().AsLowercaseString());
  EXPECT_EQ(groups[0].label, "Test Group");
  ASSERT_EQ(groups[0].tabs.size(), 1u);
  EXPECT_EQ(groups[0].tabs[0].title, "Example");
  EXPECT_EQ(groups[0].tabs[0].url, GURL("https://example.com"));

  std::optional<TabGroupEntry> fetched_group =
      service_.GetConfirmedTabGroup(group.saved_guid());
  ASSERT_TRUE(fetched_group.has_value());
  EXPECT_EQ(fetched_group->id, group.saved_guid().AsLowercaseString());
  EXPECT_EQ(fetched_group->tabs.size(), 1u);
}

TEST_F(ContextHubServiceTest, ConfirmAllTabGroups_Success) {
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
        group1->set_label("Unconfirmed Group");
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
      group_future;
  service_.GroupTabs(
      std::move(input_tabs), "",
      group_future
          .GetCallback<std::vector<TabGroupEntry>, std::vector<TabData>>());
  EXPECT_TRUE(group_future.Wait());

  base::test::TestFuture<bool, std::vector<base::Uuid>> confirm_future;
  service_.ConfirmAllTabGroups(confirm_future.GetCallback());
  auto [success, added_guids] = confirm_future.Take();

  EXPECT_TRUE(success);
  ASSERT_EQ(added_guids.size(), 1u);

  // Verify group is now in sync service
  auto confirmed_groups = fake_tab_group_sync_service_.GetAllGroups();
  ASSERT_EQ(confirmed_groups.size(), 1u);
  EXPECT_EQ(confirmed_groups[0].saved_guid(), added_guids[0]);

  // Verify store is cleared
  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future;
  service_.GetTabGroups(stored_groups_future.GetCallback());
  EXPECT_TRUE(stored_groups_future.Get().empty());
}

TEST_F(ContextHubServiceTest, TabGroupStore_Null) {
  ContextHubService service_null_store(
      &mock_personal_context_service_, &mock_remote_model_executor_,
      &fake_tab_group_sync_service_, std::make_unique<InMemoryMemoryBank>(),
      /*tab_group_store=*/nullptr,
      /*context_hub_backend=*/nullptr,
      std::make_unique<InMemoryAutoTodosStore>());

  base::test::TestFuture<std::vector<TabGroupEntry>> get_future;
  service_null_store.GetTabGroups(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());

  base::test::TestFuture<void> delete_future;
  service_null_store.DeleteAllTabGroups(delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<bool, std::vector<base::Uuid>> confirm_future;
  service_null_store.ConfirmAllTabGroups(confirm_future.GetCallback());
  auto [success, added_guids] = confirm_future.Take();
  EXPECT_FALSE(success);
  EXPECT_TRUE(added_guids.empty());
}

TEST_F(ContextHubServiceTest, GetConfirmedTabGroup_NotFound) {
  EXPECT_FALSE(service_.GetConfirmedTabGroup(base::Uuid::GenerateRandomV4())
                   .has_value());
}

TEST_F(ContextHubServiceTest, GetLocalGroupIdForConfirmedGroup) {
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  group.SetLocalGroupId(local_id);
  fake_tab_group_sync_service_.AddGroup(group);

  auto result = service_.GetLocalGroupIdForConfirmedGroup(group.saved_guid());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), local_id);

  EXPECT_FALSE(
      service_.GetLocalGroupIdForConfirmedGroup(base::Uuid::GenerateRandomV4())
          .has_value());
}

TEST_F(ContextHubServiceTest, RemoveConfirmedTabGroup) {
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  fake_tab_group_sync_service_.AddGroup(group);

  EXPECT_TRUE(service_.RemoveConfirmedTabGroup(group.saved_guid()));
  EXPECT_TRUE(fake_tab_group_sync_service_.GetAllGroups().empty());

  EXPECT_FALSE(
      service_.RemoveConfirmedTabGroup(base::Uuid::GenerateRandomV4()));
}

TEST_F(ContextHubServiceTest, RemoveAllConfirmedTabGroups) {
  tab_groups::SavedTabGroup group1(u"Group 1",
                                   tab_groups::TabGroupColorId::kBlue, {},
                                   /*position=*/std::nullopt);
  tab_groups::SavedTabGroup group2(u"Group 2",
                                   tab_groups::TabGroupColorId::kRed, {},
                                   /*position=*/std::nullopt);
  fake_tab_group_sync_service_.AddGroup(group1);
  fake_tab_group_sync_service_.AddGroup(group2);
  ASSERT_EQ(fake_tab_group_sync_service_.GetAllGroups().size(), 2u);

  EXPECT_TRUE(service_.RemoveAllConfirmedTabGroups());
  EXPECT_TRUE(fake_tab_group_sync_service_.GetAllGroups().empty());
}

TEST_F(ContextHubServiceTest, ConnectLocalTabGroup) {
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt);
  fake_tab_group_sync_service_.AddGroup(group);

  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  service_.ConnectLocalTabGroup(group.saved_guid(), local_id);

  auto updated_group = service_.GetConfirmedTabGroup(group.saved_guid());
  ASSERT_TRUE(updated_group.has_value());
  EXPECT_EQ(service_.GetLocalGroupIdForConfirmedGroup(group.saved_guid()),
            local_id);
}

}  // namespace
}  // namespace context_hub
