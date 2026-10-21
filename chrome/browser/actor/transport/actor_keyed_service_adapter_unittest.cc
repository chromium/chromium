// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/actor_keyed_service_adapter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_enterprise_policy_checker.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class ActorKeyedServiceAdapterTest : public testing::Test {
 public:
  ActorKeyedServiceAdapterTest()
      : profile_(TestingProfile::Builder().Build()),
        actor_service_(ActorKeyedService::Get(profile_.get())),
        enterprise_policy_checker_(profile_.get()),
        adapter_(actor_service_, &enterprise_policy_checker_) {}
  ~ActorKeyedServiceAdapterTest() override = default;

  void TearDown() override { actor_service_->Shutdown(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ActorKeyedService> actor_service_;
  ActorEnterprisePolicyChecker enterprise_policy_checker_;
  ActorKeyedServiceAdapter adapter_;
};

TEST_F(ActorKeyedServiceAdapterTest, StartTaskCreatesBrowserActuatorTask) {
  proto::StartTaskRequest request;
  base::test::TestFuture<TaskId, proto::StartTaskResponse> future;

  adapter_.StartTask(request, future.GetCallback());

  const auto& [task_id, response] = future.Get();
  EXPECT_EQ(response.status(), proto::StartTaskResponse::STATUS_OK);
  EXPECT_TRUE(task_id);

  ActorTask* task = actor_service_->GetTask(task_id);
  ASSERT_TRUE(task);
  EXPECT_EQ(task->source_info().type, TaskSourceInfo::Client::kBrowserActuator);
  EXPECT_FALSE(task->source_info().id.has_value());
  EXPECT_EQ(&task->policy_checker(), &enterprise_policy_checker_);
}

TEST_F(ActorKeyedServiceAdapterTest, StopTaskStopsExistingTaskAndReturnsTrue) {
  proto::StartTaskRequest start_request;
  base::test::TestFuture<TaskId, proto::StartTaskResponse> start_future;
  adapter_.StartTask(start_request, start_future.GetCallback());
  TaskId task_id = std::get<0>(start_future.Get());
  ASSERT_TRUE(actor_service_->GetTask(task_id));

  base::test::TestFuture<bool> stop_future;
  adapter_.StopTask(task_id, proto::StopTaskRequest(),
                    stop_future.GetCallback());
  EXPECT_TRUE(stop_future.Get());
  EXPECT_FALSE(actor_service_->GetTask(task_id));
}

TEST_F(ActorKeyedServiceAdapterTest, StopTaskReturnsFalseForUnknownTask) {
  base::test::TestFuture<bool> stop_future;
  adapter_.StopTask(TaskId(9999), proto::StopTaskRequest(),
                    stop_future.GetCallback());
  EXPECT_FALSE(stop_future.Get());
}

TEST_F(ActorKeyedServiceAdapterTest, ActRejectsUnknownTask) {
  proto::PerformActionsRequest unknown_request;
  unknown_request.add_actions()->mutable_wait()->set_wait_time_ms(0);

  base::test::TestFuture<proto::PerformActionsResponse> unknown_future;
  adapter_.Act(TaskId(9999), unknown_request, unknown_future.GetCallback());
  EXPECT_EQ(unknown_future.Get().action_status_code(),
            static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
}

TEST_F(ActorKeyedServiceAdapterTest, ActRejectsInvalidActionArguments) {
  proto::StartTaskRequest start_request;
  base::test::TestFuture<TaskId, proto::StartTaskResponse> start_future;
  adapter_.StartTask(start_request, start_future.GetCallback());
  TaskId task_id = std::get<0>(start_future.Get());

  proto::PerformActionsRequest request;
  request.add_actions()->mutable_click()->set_tab_id(1);

  base::test::TestFuture<proto::PerformActionsResponse> act_future;
  adapter_.Act(task_id, request, act_future.GetCallback());

  const auto& response = act_future.Get();
  EXPECT_EQ(response.action_status_code(),
            static_cast<int32_t>(mojom::ActionResultCode::kClickMissingTarget));
  EXPECT_EQ(response.failed_action_index(), 0);
}

TEST_F(ActorKeyedServiceAdapterTest, ActExecutesWaitActionAndBuildsResult) {
  proto::StartTaskRequest start_request;
  base::test::TestFuture<TaskId, proto::StartTaskResponse> start_future;
  adapter_.StartTask(start_request, start_future.GetCallback());
  TaskId task_id = std::get<0>(start_future.Get());

  proto::PerformActionsRequest request;
  request.add_actions()->mutable_wait()->set_wait_time_ms(0);

  base::test::TestFuture<proto::PerformActionsResponse> act_future;
  adapter_.Act(task_id, request, act_future.GetCallback());

  const auto& response = act_future.Get();
  EXPECT_EQ(response.status(), proto::PerformActionsResponse::STATUS_OK);
  EXPECT_EQ(response.action_status_code(),
            static_cast<int32_t>(mojom::ActionResultCode::kOk));
  EXPECT_FALSE(response.has_failed_action_index());
}

}  // namespace
}  // namespace actor
