// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/transport/actor_keyed_service_adapter.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_enterprise_policy_checker.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

class ActorKeyedServiceAdapterTest : public testing::Test {
 public:
  ActorKeyedServiceAdapterTest()
      : profile_(TestingProfile::Builder().Build()),
        actor_service_(ActorKeyedService::Get(profile_.get())),
        enterprise_policy_checker_(profile_.get()),
        adapter_(actor_service_, &enterprise_policy_checker_) {}
  ~ActorKeyedServiceAdapterTest() override = default;

  void TearDown() override { actor_service_->Shutdown(); }

  std::unique_ptr<tabs::MockTabInterface> CreateMockTab() {
    auto mock_tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab, GetProfile)
        .WillByDefault(testing::Return(profile_.get()));
    actor_service_->GetSurfaceRegistry().OnTabCreated(*mock_tab);
    return mock_tab;
  }

  static optimization_guide::proto::Actions BuildActionsProto(
      TaskId task_id,
      const proto::PerformActionsRequest& request) {
    return ActorKeyedServiceAdapter::BuildActionsProto(task_id, request);
  }

  static proto::PerformActionsResponse BuildPerformActionsResponse(
      optimization_guide::proto::ActionsResult result,
      const proto::ObservationConfig* observation_config) {
    return ActorKeyedServiceAdapter::BuildPerformActionsResponse(
        std::move(result), observation_config);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ActorKeyedService> actor_service_;
  ActorEnterprisePolicyChecker enterprise_policy_checker_;
  ActorKeyedServiceAdapter adapter_;
};

namespace {

TEST_F(ActorKeyedServiceAdapterTest, StartTaskCreatesBrowserActuatorTask) {
  proto::StartTaskRequest request;
  base::test::TestFuture<TaskId, proto::StartTaskResponse> future;

  adapter_.StartTask(request, future.GetCallback());

  const auto& [task_id, response] = future.Get();
  EXPECT_EQ(response.status(), proto::StartTaskResponse::STATUS_OK);
  EXPECT_TRUE(task_id);
  EXPECT_TRUE(response.bound_surfaces().empty());

  ActorTask* task = actor_service_->GetTask(task_id);
  ASSERT_TRUE(task);
  EXPECT_EQ(task->source_info().type, TaskSourceInfo::Client::kBrowserActuator);
  EXPECT_FALSE(task->source_info().id.has_value());
  EXPECT_EQ(&task->policy_checker(), &enterprise_policy_checker_);
}

TEST_F(ActorKeyedServiceAdapterTest, StartTaskPopulatesTitleAndBoundSurfaces) {
  std::unique_ptr<tabs::MockTabInterface> mock_tab = CreateMockTab();
  const int32_t tab_id = mock_tab->GetHandle().raw_value();

  proto::StartTaskRequest request;
  request.set_task_title("Book flight");
  proto::TargetSurface* surface = request.add_target_surfaces();
  surface->set_tab_id(tab_id);
  surface->set_execution_mode(proto::TargetSurface::HEADED);

  base::test::TestFuture<TaskId, proto::StartTaskResponse> future;
  adapter_.StartTask(request, future.GetCallback());

  const auto& [task_id, response] = future.Get();
  EXPECT_EQ(response.status(), proto::StartTaskResponse::STATUS_OK);
  ASSERT_EQ(response.bound_surfaces_size(), 1);
  EXPECT_EQ(response.bound_surfaces(0).tab_id(), tab_id);
  EXPECT_EQ(response.bound_surfaces(0).execution_mode(),
            proto::TargetSurface::HEADED);

  ActorTask* task = actor_service_->GetTask(task_id);
  ASSERT_TRUE(task);
  EXPECT_EQ(task->title(), "Book flight");
  EXPECT_TRUE(task->HasTab(mock_tab->GetHandle()));
}

TEST_F(ActorKeyedServiceAdapterTest,
       StartTaskReturnsTargetTabNotFoundForUnknownTab) {
  proto::StartTaskRequest request;
  proto::TargetSurface* surface = request.add_target_surfaces();
  surface->set_tab_id(9999);
  surface->set_execution_mode(proto::TargetSurface::HEADED);

  base::test::TestFuture<TaskId, proto::StartTaskResponse> future;
  adapter_.StartTask(request, future.GetCallback());

  const auto& [task_id, response] = future.Get();
  EXPECT_FALSE(task_id);
  EXPECT_EQ(response.status(),
            proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);
  EXPECT_TRUE(response.bound_surfaces().empty());
  EXPECT_EQ(actor_service_->GetActiveTasksCount(), 0u);
}

TEST_F(ActorKeyedServiceAdapterTest, StopTaskStopsExistingTaskAndReturnsTrue) {
  proto::StartTaskRequest start_request;
  base::test::TestFuture<TaskId, proto::StartTaskResponse> start_future;
  adapter_.StartTask(start_request, start_future.GetCallback());
  TaskId task_id = std::get<0>(start_future.Get());
  ASSERT_TRUE(actor_service_->GetTask(task_id));

  ActorTask::State observed_final_state = ActorTask::State::kCreated;
  base::CallbackListSubscription subscription =
      actor_service_->AddTaskStateChangedCallback(base::BindRepeating(
          [](ActorTask::State* out_state, ActorTask& task) {
            *out_state = task.GetState();
          },
          &observed_final_state));

  proto::StopTaskRequest stop_request;
  stop_request.set_reason(proto::StopTaskRequest::TASK_COMPLETED);
  base::test::TestFuture<bool> stop_future;
  adapter_.StopTask(task_id, stop_request, stop_future.GetCallback());
  EXPECT_TRUE(stop_future.Get());
  EXPECT_FALSE(actor_service_->GetTask(task_id));
  EXPECT_EQ(observed_final_state, ActorTask::State::kFinished);
}

TEST_F(ActorKeyedServiceAdapterTest, StopTaskMapsStoppedReasonsToTaskStates) {
  const struct {
    proto::StopTaskRequest::Reason proto_reason;
    ActorTask::StoppedReason expected_stopped_reason;
    ActorTask::State expected_state;
  } kTestCases[] = {
      {proto::StopTaskRequest::USER_CANCELLED,
       ActorTask::StoppedReason::kStoppedByUser, ActorTask::State::kCancelled},
      {proto::StopTaskRequest::TIMEOUT, ActorTask::StoppedReason::kTimeout,
       ActorTask::State::kCancelled},
      {proto::StopTaskRequest::POLICY_ABORT,
       ActorTask::StoppedReason::kAbortedByPolicy, ActorTask::State::kFailed},
  };

  for (const auto& test_case : kTestCases) {
    base::test::TestFuture<TaskId, proto::StartTaskResponse> start_future;
    adapter_.StartTask(proto::StartTaskRequest(), start_future.GetCallback());
    TaskId task_id = std::get<0>(start_future.Get());

    std::optional<ActorTask::StoppedReason> observed_stopped_reason;
    ActorTask::State observed_final_state = ActorTask::State::kCreated;
    base::CallbackListSubscription subscription =
        actor_service_->AddTaskStateChangedCallback(base::BindRepeating(
            [](std::optional<ActorTask::StoppedReason>* out_reason,
               ActorTask::State* out_state, ActorTask& task) {
              *out_reason = task.stopped_reason();
              *out_state = task.GetState();
            },
            &observed_stopped_reason, &observed_final_state));

    proto::StopTaskRequest stop_request;
    stop_request.set_reason(test_case.proto_reason);
    base::test::TestFuture<bool> stop_future;
    adapter_.StopTask(task_id, stop_request, stop_future.GetCallback());
    EXPECT_TRUE(stop_future.Get());
    EXPECT_EQ(observed_stopped_reason, test_case.expected_stopped_reason);
    EXPECT_EQ(observed_final_state, test_case.expected_state);
  }
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
  EXPECT_EQ(unknown_future.Get().status(),
            proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
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
  EXPECT_EQ(response.status(),
            proto::PerformActionsResponse::STATUS_ACTION_FAILED);
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

TEST_F(ActorKeyedServiceAdapterTest,
       BuildActionsProtoMapsObservationConfigAndAgentContainerConfig) {
  proto::PerformActionsRequest none_request;
  none_request.mutable_observation_config()->set_level(
      proto::ObservationConfig::OBSERVATION_LEVEL_NONE);
  optimization_guide::proto::Actions none_actions =
      BuildActionsProto(TaskId(6), none_request);
  EXPECT_EQ(none_actions.task_id(), 6);
  EXPECT_TRUE(none_actions.skip_async_observation_collection());

  proto::PerformActionsRequest status_only_request;
  status_only_request.mutable_observation_config()->set_level(
      proto::ObservationConfig::OBSERVATION_LEVEL_STATUS_ONLY);
  optimization_guide::proto::Actions status_only_actions =
      BuildActionsProto(TaskId(7), status_only_request);
  EXPECT_EQ(status_only_actions.task_id(), 7);
  EXPECT_FALSE(status_only_actions.skip_async_observation_collection());

  proto::PerformActionsRequest full_request;
  full_request.add_actions()->mutable_wait()->set_wait_time_ms(10);
  full_request.mutable_observation_config()->set_level(
      proto::ObservationConfig::OBSERVATION_LEVEL_FULL);
  full_request.mutable_agent_container_config()
      ->add_location_rules()
      ->mutable_location()
      ->mutable_site()
      ->set_domain("example.com");

  optimization_guide::proto::Actions full_actions =
      BuildActionsProto(TaskId(8), full_request);
  EXPECT_EQ(full_actions.task_id(), 8);
  ASSERT_EQ(full_actions.actions_size(), 1);
  EXPECT_FALSE(full_actions.skip_async_observation_collection());
  EXPECT_EQ(full_actions.task_metadata()
                .security()
                .agent_container_config()
                .location_rules_size(),
            1);
}

TEST_F(ActorKeyedServiceAdapterTest,
       BuildPerformActionsResponseMapsStatusAndObservations) {
  optimization_guide::proto::ActionsResult result;
  result.set_action_result(
      static_cast<int32_t>(mojom::ActionResultCode::kUrlBlocked));
  result.set_index_of_failed_action(2);
  result.add_extra_information("step 0 ok");

  optimization_guide::proto::TabObservation* tab_obs = result.add_tabs();
  tab_obs->set_id(15);
  tab_obs->set_result(
      optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  tab_obs->set_screenshot("png_bytes");
  tab_obs->set_screenshot_mime_type("image/png");
  optimization_guide::proto::FrameData* main_frame =
      tab_obs->mutable_annotated_page_content()->mutable_main_frame_data();
  main_frame->set_url("https://example.com/checkout");

  proto::PerformActionsResponse full_response =
      BuildPerformActionsResponse(result, nullptr);
  EXPECT_EQ(full_response.status(),
            proto::PerformActionsResponse::STATUS_SECURITY_VIOLATION);
  ASSERT_TRUE(full_response.has_failed_action_index());
  EXPECT_EQ(full_response.failed_action_index(), 2);
  ASSERT_EQ(full_response.extra_information_size(), 1);
  EXPECT_EQ(full_response.extra_information(0), "step 0 ok");
  ASSERT_EQ(full_response.observations_size(), 1);
  EXPECT_EQ(full_response.observations(0).tab_id(), 15);
  EXPECT_EQ(full_response.observations(0).status(),
            proto::BrowserObservation::STATUS_OBSERVED);
  EXPECT_EQ(full_response.observations(0).current_url(),
            "https://example.com/checkout");
  ASSERT_TRUE(full_response.observations(0).has_screenshot_data());
  EXPECT_EQ(full_response.observations(0).screenshot_data().inline_data(),
            "png_bytes");
  EXPECT_EQ(full_response.observations(0).screenshot_data().mime_type(),
            "image/png");
  EXPECT_TRUE(
      full_response.observations(0).has_inline_annotated_page_content());

  proto::ObservationConfig screenshot_only_config;
  screenshot_only_config.set_level(
      proto::ObservationConfig::OBSERVATION_LEVEL_SCREENSHOT_ONLY);
  proto::PerformActionsResponse screenshot_only_response =
      BuildPerformActionsResponse(result, &screenshot_only_config);
  ASSERT_EQ(screenshot_only_response.observations_size(), 1);
  EXPECT_EQ(
      screenshot_only_response.observations(0).screenshot_data().inline_data(),
      "png_bytes");
  EXPECT_FALSE(screenshot_only_response.observations(0)
                   .has_inline_annotated_page_content());

  proto::ObservationConfig apc_only_config;
  apc_only_config.set_level(
      proto::ObservationConfig::OBSERVATION_LEVEL_APC_ONLY);
  proto::PerformActionsResponse apc_only_response =
      BuildPerformActionsResponse(result, &apc_only_config);
  ASSERT_EQ(apc_only_response.observations_size(), 1);
  EXPECT_FALSE(apc_only_response.observations(0).has_screenshot_data());
  EXPECT_TRUE(
      apc_only_response.observations(0).has_inline_annotated_page_content());

  proto::ObservationConfig status_only_config;
  status_only_config.set_level(
      proto::ObservationConfig::OBSERVATION_LEVEL_STATUS_ONLY);
  proto::PerformActionsResponse status_only_response =
      BuildPerformActionsResponse(result, &status_only_config);
  ASSERT_EQ(status_only_response.observations_size(), 1);
  EXPECT_EQ(status_only_response.observations(0).tab_id(), 15);
  EXPECT_EQ(status_only_response.observations(0).status(),
            proto::BrowserObservation::STATUS_OBSERVED);
  EXPECT_EQ(status_only_response.observations(0).current_url(),
            "https://example.com/checkout");
  EXPECT_EQ(status_only_response.observations(0).screenshot_case(),
            proto::BrowserObservation::SCREENSHOT_NOT_SET);
  EXPECT_EQ(status_only_response.observations(0).page_content_case(),
            proto::BrowserObservation::PAGE_CONTENT_NOT_SET);

  proto::ObservationConfig none_config;
  none_config.set_level(proto::ObservationConfig::OBSERVATION_LEVEL_NONE);
  proto::PerformActionsResponse none_response =
      BuildPerformActionsResponse(result, &none_config);
  EXPECT_TRUE(none_response.observations().empty());
}

TEST_F(ActorKeyedServiceAdapterTest,
       BuildPerformActionsResponseMapsObservationErrors) {
  optimization_guide::proto::ActionsResult result;
  result.set_action_result(static_cast<int32_t>(mojom::ActionResultCode::kOk));

  optimization_guide::proto::TabObservation* crashed_tab = result.add_tabs();
  crashed_tab->set_id(10);
  crashed_tab->set_result(
      optimization_guide::proto::TabObservation::TAB_OBSERVATION_PAGE_CRASHED);

  optimization_guide::proto::TabObservation* timeout_tab = result.add_tabs();
  timeout_tab->set_id(20);
  timeout_tab->set_result(
      optimization_guide::proto::TabObservation::TAB_OBSERVATION_FETCH_ERROR);
  timeout_tab->set_screenshot_result(
      optimization_guide::proto::TabObservation::SCREENSHOT_TIMEOUT);
  timeout_tab->set_annotated_page_content_result(
      optimization_guide::proto::TabObservation::ANNOTATED_PAGE_CONTENT_ERROR);

  optimization_guide::proto::TabObservation* unset_fetch_error_tab =
      result.add_tabs();
  unset_fetch_error_tab->set_id(30);
  unset_fetch_error_tab->set_result(
      optimization_guide::proto::TabObservation::TAB_OBSERVATION_FETCH_ERROR);

  proto::PerformActionsResponse response =
      BuildPerformActionsResponse(result, nullptr);
  ASSERT_EQ(response.observations_size(), 3);
  EXPECT_EQ(response.observations(0).tab_id(), 10);
  EXPECT_EQ(response.observations(0).status(),
            proto::BrowserObservation::STATUS_PAGE_CRASHED);
  EXPECT_FALSE(response.observations(0).has_screenshot_data());
  EXPECT_FALSE(response.observations(0).has_screenshot_error());

  EXPECT_EQ(response.observations(1).tab_id(), 20);
  EXPECT_EQ(response.observations(1).status(),
            proto::BrowserObservation::STATUS_OBSERVED);
  ASSERT_TRUE(response.observations(1).has_screenshot_error());
  EXPECT_EQ(response.observations(1).screenshot_error().code(),
            proto::ObservationError::CODE_TIMEOUT);
  ASSERT_TRUE(response.observations(1).has_page_content_error());
  EXPECT_EQ(response.observations(1).page_content_error().code(),
            proto::ObservationError::CODE_FAILED);

  EXPECT_EQ(response.observations(2).tab_id(), 30);
  EXPECT_EQ(response.observations(2).status(),
            proto::BrowserObservation::STATUS_OBSERVED);
  ASSERT_TRUE(response.observations(2).has_screenshot_error());
  EXPECT_EQ(response.observations(2).screenshot_error().code(),
            proto::ObservationError::CODE_FAILED);
  ASSERT_TRUE(response.observations(2).has_page_content_error());
  EXPECT_EQ(response.observations(2).page_content_error().code(),
            proto::ObservationError::CODE_FAILED);
}

}  // namespace
}  // namespace actor
