// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <optional>

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/noop_memory_bank.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
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

}  // namespace
}  // namespace context_hub
