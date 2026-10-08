// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/actor_keyed_service_adapter.h"

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class ActorKeyedServiceAdapterTest : public testing::Test {
 public:
  ActorKeyedServiceAdapterTest() = default;
  ~ActorKeyedServiceAdapterTest() override = default;

  void TearDown() override { actor_service_.Shutdown(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ActorKeyedService actor_service_{&profile_};
  ActorKeyedServiceAdapter adapter_{&actor_service_};
};

TEST_F(ActorKeyedServiceAdapterTest, StartTaskCreatesBrowserActuatorTask) {
  optimization_guide::proto::BrowserStartTask request;
  base::test::TestFuture<optimization_guide::proto::BrowserStartTaskResult>
      future;

  adapter_.StartTask("session_alpha", request, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_EQ(result.status(),
            optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  EXPECT_GT(result.task_id(), 0);
  EXPECT_EQ(result.tab_id(), 0);

  ActorTask* task = actor_service_.GetTask(TaskId(result.task_id()));
  ASSERT_TRUE(task);
  EXPECT_EQ(task->source_info().type, TaskSourceInfo::Client::kBrowserActuator);
  ASSERT_TRUE(task->source_info().id.has_value());
  EXPECT_EQ(*task->source_info().id, "session_alpha");
}

TEST_F(ActorKeyedServiceAdapterTest, StartTaskPropagatesTabId) {
  optimization_guide::proto::BrowserStartTask request;
  request.set_tab_id(99);
  base::test::TestFuture<optimization_guide::proto::BrowserStartTaskResult>
      future;

  adapter_.StartTask("session_beta", request, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_EQ(result.status(),
            optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  EXPECT_GT(result.task_id(), 0);
  EXPECT_EQ(result.tab_id(), 99);
}

TEST_F(ActorKeyedServiceAdapterTest, StopTaskStopsExistingTaskAndReturnsTrue) {
  optimization_guide::proto::BrowserStartTask start_request;
  base::test::TestFuture<optimization_guide::proto::BrowserStartTaskResult>
      start_future;
  adapter_.StartTask("session_alpha", start_request,
                     start_future.GetCallback());
  TaskId task_id(start_future.Get().task_id());
  ASSERT_TRUE(actor_service_.GetTask(task_id));

  base::test::TestFuture<bool> stop_future;
  adapter_.StopTask(task_id, stop_future.GetCallback());
  EXPECT_TRUE(stop_future.Get());
  EXPECT_FALSE(actor_service_.GetTask(task_id));
}

TEST_F(ActorKeyedServiceAdapterTest, StopTaskReturnsFalseForUnknownTask) {
  base::test::TestFuture<bool> stop_future;
  adapter_.StopTask(TaskId(9999), stop_future.GetCallback());
  EXPECT_FALSE(stop_future.Get());
}

}  // namespace
}  // namespace actor
