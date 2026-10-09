// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service_proto_wrapper.h"

#include <utility>
#include <vector>

#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_observation_controller.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

class ActorKeyedServiceProtoWrapperTest : public testing::Test {
 public:
  ActorKeyedServiceProtoWrapperTest()
      : profile_(TestingProfile::Builder().Build()),
        actor_service_(ActorKeyedService::Get(profile_.get())),
        wrapper_(actor_service_) {}
  ~ActorKeyedServiceProtoWrapperTest() override = default;

 protected:
  TaskId CreateTask() {
    return actor_service_->CreateTask(
        TaskSourceInfo(TaskSourceInfo::Client::kTest, "test_session"),
        GetNullEnterprisePolicyChecker());
  }

  static optimization_guide::proto::ActionsResult BuildActionsResult(
      base::TimeTicks start_time,
      const std::vector<ActionResultWithLatencyInfo>& action_results,
      ObservationResult& observation_result) {
    return ActorKeyedServiceProtoWrapper::BuildActionsResult(
        start_time, action_results, observation_result);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ActorKeyedService> actor_service_;
  ActorKeyedServiceProtoWrapper wrapper_;
};

namespace {

TEST_F(ActorKeyedServiceProtoWrapperTest, PerformActionsRejectsUnknownTask) {
  optimization_guide::proto::Actions unknown_actions;
  unknown_actions.set_task_id(9999);
  unknown_actions.add_actions()->mutable_wait()->set_wait_time_ms(0);

  base::test::TestFuture<optimization_guide::proto::ActionsResult>
      unknown_future;
  wrapper_.PerformActions(unknown_actions, unknown_future.GetCallback());
  EXPECT_EQ(unknown_future.Get().action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
}

TEST_F(ActorKeyedServiceProtoWrapperTest,
       PerformActionsRejectsInvalidActionArguments) {
  TaskId task_id = CreateTask();

  optimization_guide::proto::Actions actions;
  actions.set_task_id(task_id.value());
  actions.add_actions()->mutable_click()->set_tab_id(1);

  base::test::TestFuture<optimization_guide::proto::ActionsResult> future;
  wrapper_.PerformActions(actions, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_EQ(result.action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kClickMissingTarget));
  ASSERT_TRUE(result.has_index_of_failed_action());
  EXPECT_EQ(result.index_of_failed_action(), 0);
}

TEST_F(ActorKeyedServiceProtoWrapperTest,
       PerformActionsRunsWaitActionAndBuildsResult) {
  TaskId task_id = CreateTask();

  optimization_guide::proto::Actions actions;
  actions.set_task_id(task_id.value());
  actions.add_actions()->mutable_wait()->set_wait_time_ms(0);

  base::test::TestFuture<optimization_guide::proto::ActionsResult> future;
  wrapper_.PerformActions(actions, future.GetCallback());

  const auto& result = future.Get();
  EXPECT_EQ(result.action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kOk));
  EXPECT_FALSE(result.has_index_of_failed_action());
  EXPECT_GE(result.latency_information().latency_steps_size(), 1);
}

TEST_F(ActorKeyedServiceProtoWrapperTest,
       BuildActionsResultPopulatesObservationsAndLatencies) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  mojom::ActionResultPtr ok_result =
      MakeOkResultWithMessage(/*requires_page_stabilization=*/true, "done");
  ok_result->execution_end_time = start_time + base::Milliseconds(25);

  std::vector<ActionResultWithLatencyInfo> action_results;
  action_results.emplace_back(start_time + base::Milliseconds(5),
                              start_time + base::Milliseconds(40),
                              std::move(ok_result));

  ObservationResult observation_result;
  optimization_guide::proto::TabObservation tab_obs;
  tab_obs.set_id(77);
  tab_obs.set_result(
      optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  tab_obs.mutable_annotated_page_content()
      ->mutable_main_frame_data()
      ->set_title("apc");
  tab_obs.set_screenshot("png_bytes");
  observation_result.tab_observations.push_back(std::move(tab_obs));

  optimization_guide::proto::WindowObservation win_obs;
  win_obs.set_id(11);
  win_obs.set_active(true);
  win_obs.set_activated_tab_id(77);
  observation_result.window_observations.push_back(std::move(win_obs));

  optimization_guide::proto::ActionsResult_LatencyInformation_LatencyStep
      apc_step;
  apc_step.set_latency_start_ms(40);
  apc_step.set_latency_stop_ms(55);
  apc_step.mutable_annotated_page_content()->set_id(77);
  observation_result.latency_steps.push_back(std::move(apc_step));

  optimization_guide::proto::ActionsResult result =
      BuildActionsResult(start_time, action_results, observation_result);

  EXPECT_EQ(result.action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kOk));
  EXPECT_FALSE(result.has_index_of_failed_action());
  ASSERT_EQ(result.extra_information_size(), 1);
  EXPECT_EQ(result.extra_information(0), "done");
  ASSERT_EQ(result.tabs_size(), 1);
  EXPECT_EQ(result.tabs(0).id(), 77);
  EXPECT_EQ(result.tabs(0).annotated_page_content().main_frame_data().title(),
            "apc");
  EXPECT_EQ(result.tabs(0).screenshot(), "png_bytes");
  ASSERT_EQ(result.windows_size(), 1);
  EXPECT_EQ(result.windows(0).id(), 11);
  ASSERT_EQ(result.latency_information().latency_steps_size(), 3);
  EXPECT_EQ(result.latency_information().latency_steps(0).latency_start_ms(),
            5);
  EXPECT_EQ(result.latency_information().latency_steps(0).latency_stop_ms(),
            25);
  EXPECT_EQ(result.latency_information().latency_steps(1).latency_start_ms(),
            25);
  EXPECT_EQ(result.latency_information().latency_steps(1).latency_stop_ms(),
            40);
  EXPECT_EQ(result.latency_information().latency_steps(2).latency_start_ms(),
            40);
  EXPECT_EQ(result.latency_information().latency_steps(2).latency_stop_ms(),
            55);
}

TEST_F(ActorKeyedServiceProtoWrapperTest,
       BuildActionsResultPropagatesErrorDetails) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  mojom::ActionResultPtr err_result =
      MakeResult(mojom::ActionResultCode::kTabWentAway,
                 /*requires_page_stabilization=*/false, "tab closed");
  err_result->execution_end_time = start_time + base::Milliseconds(10);

  std::vector<ActionResultWithLatencyInfo> action_results;
  action_results.emplace_back(start_time, start_time + base::Milliseconds(10),
                              std::move(err_result));

  ObservationResult observation_result;
  optimization_guide::proto::ActionsResult result =
      BuildActionsResult(start_time, action_results, observation_result);

  EXPECT_EQ(result.action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kTabWentAway));
  EXPECT_EQ(result.index_of_failed_action(), 0);
  EXPECT_EQ(result.error_message(), "tab closed");
  ASSERT_EQ(result.extra_information_size(), 1);
  EXPECT_TRUE(result.extra_information(0).empty());
}

}  // namespace
}  // namespace actor
