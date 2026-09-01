// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_critical_action_logger.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

class ActorCriticalActionLoggerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {critical_actions::features::kCriticalActionHistory,
         features::kGlicActor},
        {});
    ChromeRenderViewHostTestHarness::SetUp();
    service_ = std::make_unique<ActorKeyedServiceFake>(profile());
  }

  void TearDown() override {
    service_.reset();
    tab_states_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ActorKeyedServiceFake& actor_service() { return *service_; }

  tabs::TabHandle CreateTabHandle() {
    tab_states_.push_back(std::make_unique<TestTabState>(web_contents()));
    return tab_states_.back()->tab.GetHandle();
  }

  void FlushPendingActions(int64_t navigation_id) {
    critical_actions::CriticalActionService* critical_service =
        critical_actions::CriticalActionFactory::GetForProfile(profile());
    ASSERT_TRUE(critical_service);
    history::URLRow url_row;
    history::VisitRow visit_row;
    visit_row.visit_id = 42;
    history::VisitedURLInfo visited_info(
        url_row, visit_row, history::VisitResponseCodeCategory::kNot404,
        navigation_id);
    critical_service->OnURLVisitedWithNavigationId(nullptr, visited_info);
  }

  std::vector<critical_actions::CriticalActionEntry> GetLoggedActions() {
    critical_actions::CriticalActionService* critical_service =
        critical_actions::CriticalActionFactory::GetForProfile(profile());
    if (!critical_service) {
      return {};
    }
    base::test::TestFuture<std::vector<critical_actions::CriticalActionEntry>>
        future;
    critical_actions::CriticalActionQueryOptions options;
    critical_service->GetCriticalActions(options, future.GetCallback());
    return future.Get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ActorKeyedServiceFake> service_;
  std::vector<std::unique_ptr<TestTabState>> tab_states_;
};

TEST_F(ActorCriticalActionLoggerTest, LogsAttemptLoginAsPasswordManagerAction) {
  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget password_button;
  AttemptLoginToolRequest request(CreateTabHandle(), password_button,
                                  std::nullopt);
  mojom::ActionResultPtr result = MakeOkResult();
  result->attempt_login_status = mojom::AttemptLoginStatus::kPasswordManager;

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1001);
  FlushPendingActions(1001);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kGooglePasswordManager);
  EXPECT_EQ(logged_actions[0].conversation_id,
            task->source_info().id.value_or(""));
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, LogsAttemptLoginAsFederatedLoginAction) {
  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget google_button;
  AttemptLoginToolRequest request(CreateTabHandle(), std::nullopt,
                                  google_button);
  mojom::ActionResultPtr result = MakeOkResult();
  result->attempt_login_status = mojom::AttemptLoginStatus::kFederated;

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1002);
  FlushPendingActions(1002);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kFederatedLogin);
  EXPECT_EQ(logged_actions[0].conversation_id,
            task->source_info().id.value_or(""));
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, SkipsAttemptLoginWhenNoStatusResult) {
  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget password_button;
  AttemptLoginToolRequest request(CreateTabHandle(), password_button,
                                  std::nullopt);
  // Login action returned result, but attempt_login_status is std::nullopt
  // (e.g. fail/unsupported)
  mojom::ActionResultPtr result = MakeOkResult();
  result->attempt_login_status = std::nullopt;

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1003);
  FlushPendingActions(1003);

  auto logged_actions = GetLoggedActions();
  EXPECT_TRUE(logged_actions.empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, LogsAttemptOtpFillingAction) {
  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget otp_input;
  AttemptOtpFillingToolRequest request(CreateTabHandle(), {otp_input},
                                       /*for_signin=*/true);
  mojom::ActionResultPtr result = MakeOkResult();

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1004);
  FlushPendingActions(1004);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kCredentialsOtp);
  EXPECT_EQ(logged_actions[0].conversation_id,
            task->source_info().id.value_or(""));
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, LogsFormFillActionWithMetadata) {
  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  AttemptFormFillingToolRequest::FormFillingRequest sub_req1;
  sub_req1.requested_data =
      autofill::ActorFormFillingRequestedData::kCreditCard;

  AttemptFormFillingToolRequest::FormFillingRequest sub_req2;
  sub_req2.requested_data = autofill::ActorFormFillingRequestedData::kAddress;

  AttemptFormFillingToolRequest request(CreateTabHandle(), {sub_req1, sub_req2},
                                        /*enqueued_click=*/true);
  mojom::ActionResultPtr result = MakeOkResult();

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1002);
  FlushPendingActions(1002);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kFormFill);
  EXPECT_EQ(logged_actions[0].conversation_id,
            task->source_info().id.value_or(""));
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));
  EXPECT_EQ(logged_actions[0].metadata,
            "{\"requested_data\":[\"kCreditCard\",\"kAddress\"]}");

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, LogsAgentSelfReportedActionDirectly) {
  ActorCriticalActionLogger::LogAgentSelfReportedAction(
      profile(), "conversation-123", critical_actions::ActionType::kDownload,
      GURL("https://example.com/file.pdf"),
      /*navigation_id=*/1008, TaskId(123));
  FlushPendingActions(1008);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);

  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kDownload);
  EXPECT_EQ(logged_actions[0].conversation_id, "conversation-123");
  EXPECT_EQ(logged_actions[0].actor_task_id, "123");
  EXPECT_EQ(logged_actions[0].url, GURL("https://example.com/file.pdf"));
}

TEST_F(ActorCriticalActionLoggerTest, SkipsNonCriticalAction) {
  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  FakeToolRequest request(base::DoNothing(), base::DoNothing());
  mojom::ActionResultPtr result = MakeOkResult();

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1010);
  FlushPendingActions(1010);

  auto logged_actions = GetLoggedActions();
  EXPECT_TRUE(logged_actions.empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, HandlesNullProfile) {
  // Call with a null profile pointer. This should return early without logging
  // and without crashing.
  ActorCriticalActionLogger::LogAgentSelfReportedAction(
      /*profile=*/nullptr, "conversation-123",
      critical_actions::ActionType::kDownload,
      GURL("https://example.com/file.pdf"),
      /*navigation_id=*/1008, TaskId(123));

  auto logged_actions = GetLoggedActions();
  EXPECT_TRUE(logged_actions.empty());
}

TEST_F(ActorCriticalActionLoggerTest, SkipsFailedToolExecution) {
  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget otp_input;
  AttemptOtpFillingToolRequest request(CreateTabHandle(), {otp_input},
                                       /*for_signin=*/true);
  mojom::ActionResultPtr result = mojom::ActionResult::New();
  result->code = mojom::ActionResultCode::kFrameWentAway;

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1011);
  FlushPendingActions(1011);

  auto logged_actions = GetLoggedActions();
  EXPECT_TRUE(logged_actions.empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, FormFillingLoggingPreClickGating) {
  base::test::ScopedFeatureList local_features(
      features::kGlicActorAutofillPreClick);

  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  AttemptFormFillingToolRequest::FormFillingRequest sub_req;
  sub_req.requested_data = autofill::ActorFormFillingRequestedData::kAddress;

  // 1. If enqueued_click = false (pre-click), it should NOT log.
  AttemptFormFillingToolRequest pre_click_request(CreateTabHandle(), {sub_req},
                                                  /*enqueued_click=*/false);
  mojom::ActionResultPtr result = MakeOkResult();
  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), pre_click_request,
                                            *result, /*navigation_id=*/1012);
  FlushPendingActions(1012);
  EXPECT_TRUE(GetLoggedActions().empty());

  // 2. If enqueued_click = true, it SHOULD log.
  AttemptFormFillingToolRequest click_request(CreateTabHandle(), {sub_req},
                                              /*enqueued_click=*/true);
  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), click_request,
                                            *result, /*navigation_id=*/1013);
  FlushPendingActions(1013);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kFormFill);

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, SkipsLoggingWhenFeatureDisabled) {
  base::test::ScopedFeatureList local_features;
  local_features.InitAndDisableFeature(
      critical_actions::features::kCriticalActionHistory);

  TaskId task_id = actor_service().CreateTaskForTesting();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget password_button;
  AttemptLoginToolRequest request(CreateTabHandle(), password_button,
                                  std::nullopt);
  mojom::ActionResultPtr result = MakeOkResult();
  result->attempt_login_status = mojom::AttemptLoginStatus::kPasswordManager;

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                             /*navigation_id=*/1001);

  auto logged_actions = GetLoggedActions();
  EXPECT_TRUE(logged_actions.empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

}  // namespace

}  // namespace actor
