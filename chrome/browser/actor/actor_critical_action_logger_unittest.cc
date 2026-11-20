// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_critical_action_logger.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_options.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/autofill/attempt_form_filling/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/password_manager/attempt_login/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
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
#include "components/download/public/common/mock_download_item.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"

namespace actor {

namespace {

using ::testing::HasSubstr;
using ::testing::Not;

constexpr char kConversationId[] = "conversation-123";

class ActorCriticalActionLoggerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {critical_actions::features::kCriticalActionHistory,
         features::kGlicActor},
        {});
    ChromeRenderViewHostTestHarness::SetUp();
    service_ = static_cast<ActorKeyedServiceFake*>(
        ActorKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<ActorKeyedServiceFake>(
                  Profile::FromBrowserContext(context));
            })));
  }

  void TearDown() override {
    service_ = nullptr;
    tab_states_.clear();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ActorKeyedServiceFake& actor_service() { return *service_; }

  // Creates a task created by Glic, the only source whose critical actions are
  // logged.
  TaskId CreateGlicTask(ActorTaskOptions options = {}) {
    return actor_service().CreateTaskForTesting(
        TaskSourceInfo(TaskSourceInfo::Client::kGlic, kConversationId),
        std::move(options));
  }

  ActorSurfaceHandle CreateActorSurfaceHandle() {
    tab_states_.push_back(std::make_unique<TestTabState>(web_contents()));
    return ActorSurfaceHandle::From(tab_states_.back()->tab.GetHandle());
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
  raw_ptr<ActorKeyedServiceFake> service_ = nullptr;
  std::vector<std::unique_ptr<TestTabState>> tab_states_;
};

TEST_F(ActorCriticalActionLoggerTest, LogsAttemptLoginAsPasswordManagerAction) {
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget password_button;
  AttemptLoginToolRequest request(CreateActorSurfaceHandle(), password_button,
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
  EXPECT_EQ(logged_actions[0].conversation_id, kConversationId);
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, LogsAttemptLoginAsFederatedLoginAction) {
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget google_button;
  AttemptLoginToolRequest request(CreateActorSurfaceHandle(), std::nullopt,
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
  EXPECT_EQ(logged_actions[0].conversation_id, kConversationId);
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, SkipsAttemptLoginWhenNoStatusResult) {
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget password_button;
  AttemptLoginToolRequest request(CreateActorSurfaceHandle(), password_button,
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
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget otp_input;
  AttemptOtpFillingToolRequest request(CreateActorSurfaceHandle(), {otp_input},
                                       /*for_signin=*/true);
  mojom::ActionResultPtr result = MakeOkResult();

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1004);
  FlushPendingActions(1004);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kCredentialsOtp);
  EXPECT_EQ(logged_actions[0].conversation_id, kConversationId);
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, LogsFormFillActionWithMetadata) {
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  AttemptFormFillingToolRequest::FormFillingRequest sub_req1;
  sub_req1.requested_data =
      autofill::ActorFormFillingRequestedData::kCreditCard;

  AttemptFormFillingToolRequest::FormFillingRequest sub_req2;
  sub_req2.requested_data = autofill::ActorFormFillingRequestedData::kAddress;

  AttemptFormFillingToolRequest request(CreateActorSurfaceHandle(),
                                        {sub_req1, sub_req2},
                                        /*credit_card_opaque_token=*/"",
                                        /*enqueued_click=*/true);
  mojom::ActionResultPtr result = MakeOkResult();

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1002);
  FlushPendingActions(1002);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kFormFill);
  EXPECT_EQ(logged_actions[0].conversation_id, kConversationId);
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));
  EXPECT_EQ(logged_actions[0].metadata,
            "{\"requested_data\":[\"kCreditCard\",\"kAddress\"]}");

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, LogsAgentSelfReportedActionDirectly) {
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  ActorCriticalActionLogger::LogAgentSelfReportedAction(
      profile(), *task, critical_actions::ActionType::kDownload,
      /*navigation_id=*/1008);
  FlushPendingActions(1008);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);

  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kDownload);
  EXPECT_EQ(logged_actions[0].conversation_id, kConversationId);
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, SkipsNonCriticalAction) {
  TaskId task_id = CreateGlicTask();
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
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  // Call with a null profile pointer. This should return early without logging
  // and without crashing.
  ActorCriticalActionLogger::LogAgentSelfReportedAction(
      /*profile=*/nullptr, *task, critical_actions::ActionType::kDownload,
      /*navigation_id=*/1008);
  FlushPendingActions(1008);

  auto logged_actions = GetLoggedActions();
  EXPECT_TRUE(logged_actions.empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest, SkipsFailedToolExecution) {
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget otp_input;
  AttemptOtpFillingToolRequest request(CreateActorSurfaceHandle(), {otp_input},
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

  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  AttemptFormFillingToolRequest::FormFillingRequest sub_req;
  sub_req.requested_data = autofill::ActorFormFillingRequestedData::kAddress;

  // 1. If enqueued_click = false (pre-click), it should NOT log.
  AttemptFormFillingToolRequest pre_click_request(
      CreateActorSurfaceHandle(), {sub_req}, /*credit_card_opaque_token=*/"",
      /*enqueued_click=*/false);
  mojom::ActionResultPtr result = MakeOkResult();
  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), pre_click_request,
                                            *result, /*navigation_id=*/1012);
  FlushPendingActions(1012);
  EXPECT_TRUE(GetLoggedActions().empty());

  // 2. If enqueued_click = true, it SHOULD log.
  AttemptFormFillingToolRequest click_request(CreateActorSurfaceHandle(),
                                              {sub_req},
                                              /*credit_card_opaque_token=*/"",
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

  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget password_button;
  AttemptLoginToolRequest request(CreateActorSurfaceHandle(), password_button,
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

TEST_F(ActorCriticalActionLoggerTest, LogsWebMcpToolAction) {
  TaskId task_id = CreateGlicTask();
  ActorTask* task = actor_service().GetTask(task_id);

  ScriptToolRequest request(CreateActorSurfaceHandle(),
                            base::UnguessableToken::Create(), "flight_search",
                            "{\"destination\":\"Paris\"}");

  mojom::ActionResultPtr result = MakeOkResult();
  result->script_tool_response = mojom::ScriptToolResponse::New();
  result->script_tool_response->input_arguments = "{\"destination\":\"Paris\"}";
  result->script_tool_response->tool = blink::mojom::ScriptTool::New();
  result->script_tool_response->tool->name = "flight_search";
  result->script_tool_response->tool->title = "Search Flights";
  result->script_tool_response->tool->description =
      "Searches and books flight tickets";
  result->script_tool_response->tool->annotations =
      blink::mojom::ScriptToolAnnotations::New();
  result->script_tool_response->tool->annotations->consequential = true;
  result->script_tool_response->tool->annotations->read_only = false;

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1014);
  FlushPendingActions(1014);

  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kWebMcpTool);
  EXPECT_EQ(logged_actions[0].conversation_id, kConversationId);
  EXPECT_EQ(logged_actions[0].actor_task_id,
            base::NumberToString(task_id.value()));
  EXPECT_THAT(logged_actions[0].metadata,
              HasSubstr("\"tool_name\":\"flight_search\""));
  EXPECT_THAT(logged_actions[0].metadata,
              HasSubstr("\"tool_title\":\"Search Flights\""));
  EXPECT_THAT(
      logged_actions[0].metadata,
      HasSubstr("\"tool_description\":\"Searches and books flight tickets\""));
  EXPECT_THAT(logged_actions[0].metadata, HasSubstr("\"consequential\":true"));
  EXPECT_THAT(logged_actions[0].metadata, HasSubstr("\"read_only\":false"));
  EXPECT_THAT(logged_actions[0].metadata, Not(HasSubstr("input_arguments")));

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

class ActorCriticalActionLoggerSourceTest
    : public ActorCriticalActionLoggerTest,
      public testing::WithParamInterface<TaskSourceInfo::Client> {};

// Critical actions of tasks not created by Glic must not be logged, neither
// from tool execution nor from self-reported actions (e.g. downloads).
TEST_P(ActorCriticalActionLoggerSourceTest, SkipsNonGlicTasks) {
  // Arbitrary navigation id; pending entries are flushed when a visit with this
  // id is recorded.
  constexpr int64_t kNavigationId = 2001;

  TaskId task_id = actor_service().CreateTaskWithSourceInfoForTesting(
      TaskSourceInfo(GetParam(), "some-id"));
  ActorTask* task = actor_service().GetTask(task_id);

  PageTarget otp_input;
  AttemptOtpFillingToolRequest request(CreateActorSurfaceHandle(), {otp_input},
                                       /*for_signin=*/true);
  mojom::ActionResultPtr result = MakeOkResult();
  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            kNavigationId);
  ActorCriticalActionLogger::LogAgentSelfReportedAction(
      profile(), *task, critical_actions::ActionType::kDownload, kNavigationId);
  FlushPendingActions(kNavigationId);

  EXPECT_TRUE(GetLoggedActions().empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ActorCriticalActionLoggerSourceTest,
    testing::Values(TaskSourceInfo::Client::kUnknown,
                    TaskSourceInfo::Client::kTest,
                    TaskSourceInfo::Client::kExperimentalActor,
                    TaskSourceInfo::Client::kContextualTasks,
                    TaskSourceInfo::Client::kTtc,
                    TaskSourceInfo::Client::kBrowserActuator));

TEST_F(ActorCriticalActionLoggerTest,
       SkipsMaybeLogActionWhenSkipHistoryRecordingIsTrue) {
  ActorTaskOptions options;
  options.skip_history_recording = true;
  TaskId task_id = CreateGlicTask(std::move(options));
  ActorTask* task = actor_service().GetTask(task_id);
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->skip_history_recording());

  PageTarget password_button;
  AttemptLoginToolRequest request(CreateActorSurfaceHandle(), password_button,
                                  std::nullopt);
  mojom::ActionResultPtr result = MakeOkResult();
  result->attempt_login_status = mojom::AttemptLoginStatus::kPasswordManager;

  ActorCriticalActionLogger::MaybeLogAction(*task, profile(), request, *result,
                                            /*navigation_id=*/1015);
  FlushPendingActions(1015);

  EXPECT_TRUE(GetLoggedActions().empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest,
       SkipsLogAgentSelfReportedActionWhenSkipHistoryRecordingIsTrue) {
  ActorTaskOptions options;
  options.skip_history_recording = true;
  TaskId task_id = CreateGlicTask(std::move(options));
  ActorTask* task = actor_service().GetTask(task_id);
  ASSERT_TRUE(task);

  ActorCriticalActionLogger::LogAgentSelfReportedAction(
      profile(), *task, critical_actions::ActionType::kDownload,
      /*navigation_id=*/1016);
  FlushPendingActions(1016);

  EXPECT_TRUE(GetLoggedActions().empty());

  actor_service().StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorCriticalActionLoggerTest,
       OnDownloadCreatedRespectsSkipHistoryRecording) {
  NavigateAndCommit(GURL("https://example.com"));
  ActorSurfaceHandle surface_handle = CreateActorSurfaceHandle();
  int64_t nav_id = web_contents()->GetPrimaryMainFrame()->GetNavigationId();

  ActorTaskOptions skip_options;
  skip_options.skip_history_recording = true;
  TaskId skip_task_id = CreateGlicTask(std::move(skip_options));
  AddTabToTask(*surface_handle.GetTabHandle().Get(),
               *actor_service().GetTask(skip_task_id));

  testing::NiceMock<download::MockDownloadItem> download_item;
  content::DownloadItemUtils::AttachInfoForTesting(&download_item, profile(),
                                                   web_contents());

  actor_service().OnDownloadCreated(nullptr, &download_item);
  FlushPendingActions(nav_id);
  EXPECT_TRUE(GetLoggedActions().empty());

  actor_service().StopTaskForTesting(
      skip_task_id, actor::ActorTask::StoppedReason::kTaskComplete);

  TaskId normal_task_id = CreateGlicTask();
  AddTabToTask(*surface_handle.GetTabHandle().Get(),
               *actor_service().GetTask(normal_task_id));

  actor_service().OnDownloadCreated(nullptr, &download_item);
  FlushPendingActions(nav_id);
  auto logged_actions = GetLoggedActions();
  ASSERT_EQ(logged_actions.size(), 1u);
  EXPECT_EQ(logged_actions[0].action_type,
            critical_actions::ActionType::kDownload);

  actor_service().StopTaskForTesting(
      normal_task_id, actor::ActorTask::StoppedReason::kTaskComplete);
}

}  // namespace

}  // namespace actor
