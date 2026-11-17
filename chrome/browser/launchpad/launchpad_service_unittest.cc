// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/launchpad_service.h"

#include <optional>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace launchpad {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

class LaunchpadServiceTest : public testing::Test {
 public:
  LaunchpadServiceTest() : service_(&mock_personal_context_service_) {}
  ~LaunchpadServiceTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  personal_context::MockPersonalContextService mock_personal_context_service_;
  LaunchpadService service_;
};

TEST_F(LaunchpadServiceTest, GenerateTasks_ServiceSuccess) {
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
  service_.GenerateTasks(future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value().todos_size(), 1);
  EXPECT_EQ(result.value().todos(0).title(), "Test Todo");
  EXPECT_EQ(result.value().todos(0).description(), "Test Description");
}

TEST_F(LaunchpadServiceTest, GenerateTasks_ServiceError) {
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
  service_.GenerateTasks(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(LaunchpadServiceTest, GenerateTasks_ParseError) {
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
  service_.GenerateTasks(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace
}  // namespace launchpad
