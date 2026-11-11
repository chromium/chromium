// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/open_url_tool_request.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/mock_remote_model_executor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/app_browser_test_base.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "components/actor/core/task_source_info.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/optimization_guide/proto/features/streaming/ttc.pb.h"
#include "components/ttc/app/ttc_backend.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {

using base::test::TestFuture;
namespace proto = optimization_guide::proto;

namespace {

// Observes `HistoryService` and waits for the next `OnURLVisited` notification
// matching `expected_url`.
class HistoryVisitWaiter : public history::HistoryServiceObserver {
 public:
  HistoryVisitWaiter(history::HistoryService* history_service,
                     const GURL& expected_url)
      : expected_url_(expected_url) {
    CHECK(history_service);
    observation_.Observe(history_service);
  }
  ~HistoryVisitWaiter() override = default;

  history::VisitedURLInfo Wait() { return future_.Take(); }

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::VisitedURLInfo& visited_url_info) override {
    if (visited_url_info.url_row.url() == expected_url_ && !future_.IsReady()) {
      future_.SetValue(visited_url_info);
    }
  }

 private:
  const GURL expected_url_;
  TestFuture<history::VisitedURLInfo> future_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      observation_{this};
};

}  // namespace

// Tests Ttc-MES protocol interactions. Exercises the full Ttc stack, from the
// service down to the frames handed to the model execution service.
class TtcProtocolBrowserTest : public AppBrowserTestBase {
 public:
  TtcProtocolBrowserTest() = default;
  ~TtcProtocolBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest, StartSessionSendsSetupRequest) {
  // Starting a session should send a SetupRequestFrame
  TestFuture<proto::TtcClientFrame> sent_frame;
  EXPECT_CLIENT_FRAME(Send, sent_frame);

  StartSessionAndConnectBackend();

  ASSERT_TRUE(sent_frame.IsReady());
  ASSERT_TRUE(sent_frame.Get().has_setup_request());
  EXPECT_EQ(sent_frame.Get().setup_request().audio_codec(),
            proto::AUDIO_CODEC_PCM16K16BIT);
  EXPECT_EQ(sent_frame.Get().setup_request().model_preference(),
            proto::MODEL_PREFERENCE_UNSPECIFIED);

  // Note: the backend connected state is the transport layer; the session
  // isn't live until the server sets up the application.
  EXPECT_TRUE(backend()->is_transport_connected());
  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kInitializing);

  testing::Mock::VerifyAndClearExpectations(model_execution_session());

  // Respond with a successful SessionStatusFrame
  RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                       "session_1");

  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kLive);
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest,
                       ClockSyncComputesServerClockDelta) {
  const base::Time before_send = base::Time::Now();

  TestFuture<proto::TtcClientFrame> sent_frame;
  EXPECT_CLIENT_FRAME(Send, sent_frame);

  StartSessionAndConnectBackend();

  // The client timestamp of the setup request is used to compute the clock
  // delta for server journal events so ensure we're sending it.
  ASSERT_TRUE(sent_frame.IsReady());
  ASSERT_TRUE(sent_frame.Get().has_setup_request());
  const int64_t client_timestamp_ms = sent_frame.Get().client_timestamp_ms();
  EXPECT_GE(client_timestamp_ms, before_send.InMillisecondsSinceUnixEpoch());
  EXPECT_LE(client_timestamp_ms,
            base::Time::Now().InMillisecondsSinceUnixEpoch());

  const base::Time client_send_time =
      base::Time::UnixEpoch() + base::Milliseconds(client_timestamp_ms);
  constexpr base::TimeDelta kServerOffset = base::Seconds(-10);
  const base::Time server_time = client_send_time + kServerOffset;

  proto::TtcServerFrame sync_frame;
  proto::JournalEvent* journal_event = sync_frame.mutable_journal_event();
  journal_event->set_type(proto::JournalEvent::JOURNAL_EVENT_TYPE_CLOCK_SYNC);
  auto set_proto_timestamp = [](proto::Timestamp* proto_timestamp,
                                base::Time time) {
    const base::TimeDelta since_epoch = time - base::Time::UnixEpoch();
    proto_timestamp->set_seconds(since_epoch.InSeconds());
    proto_timestamp->set_nanos(
        (since_epoch - base::Seconds(since_epoch.InSeconds())).InNanoseconds());
  };
  set_proto_timestamp(journal_event->mutable_sync_timestamp(),
                      client_send_time);
  set_proto_timestamp(journal_event->mutable_timestamp(), server_time);

  const base::Time before_sync = base::Time::Now();
  SimulateResponse(sync_frame);
  const base::Time after_sync = base::Time::Now();

  const base::Time min_midpoint =
      client_send_time + (before_sync - client_send_time) / 2;
  const base::Time max_midpoint =
      client_send_time + (after_sync - client_send_time) / 2;

  const base::TimeDelta delta =
      session_controller()->GetJournal().server_clock_delta();
  EXPECT_GE(delta, min_midpoint - server_time);
  EXPECT_LE(delta, max_midpoint - server_time);
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest, SetupResponseSendsToolSet) {
  StartSessionAndConnectBackend();

  // Respond to the setup successfully. This should send a tool set update
  // frame.
  TestFuture<proto::TtcClientFrame> tool_set_frame;
  EXPECT_CLIENT_FRAME(Send, tool_set_frame);

  RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                       "session_1");

  ASSERT_TRUE(tool_set_frame.IsReady());
  EXPECT_TRUE(tool_set_frame.Get().has_toolset_update());
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest, RejectedSessionSetup) {
  // Start a session where the response comes back with a rejection due to
  // QUOTA_EXCEEDED.
  {
    StartSessionAndConnectBackend();
    RespondSessionStatus(
        proto::SessionStatusResponse::STATE_REJECTED_QUOTA_EXCEEDED,
        "session_1", "quota exceeded");

    EXPECT_TRUE(backend()->is_transport_connected());

    // TODO(b/564241442): TtcMesClient ignores the error-like SessionStates in
    // SessionStatusResponse and treats them as connected. Additionally, we
    // have the ServerErrorNotification message which seems redundant. Should
    // probably reconcile the two and make sure an error in setup is correctly
    // treated as such.
    EXPECT_EQ(session_controller()->GetSessionLifecycle(),
              SessionLifecycle::kLive);
    ttc_service().EndSession();
    ASSERT_EQ(session_controller(), nullptr);
  }

  ResetModelExecutionSession();

  // A subsequent session should be able to connect successfully.
  {
    StartSessionAndConnectBackend();
    RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                         "session_2");
    EXPECT_EQ(session_controller()->GetSessionLifecycle(),
              SessionLifecycle::kLive);
  }
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest, ServerErrorEndsSession) {
  StartSessionAndConnectBackend();
  RespondServerError(
      proto::ServerErrorNotification::ERROR_CODE_INTERNAL_BACKEND_ERROR,
      "internal backend error");

  // An error frame doesn't itself drop the connection; that happens when the
  // session is torn down.
  EXPECT_TRUE(backend()->is_transport_connected());
  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kFinished);

  // The session itself is torn down asynchronously.
  EXPECT_TRUE(ttc_service().is_session_active());
  ASSERT_TRUE(WaitForServiceState(ServiceState::kSessionInactive));
  EXPECT_EQ(session_controller(), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest, NewSessionAfterServerError) {
  // Start a session that responds with a server error frame.
  {
    StartSessionAndConnectBackend();
    RespondServerError(
        proto::ServerErrorNotification::ERROR_CODE_INTERNAL_BACKEND_ERROR,
        "internal backend error");
    ASSERT_TRUE(WaitForServiceState(ServiceState::kSessionInactive));
    ASSERT_EQ(session_controller(), nullptr);
  }

  ResetModelExecutionSession();

  // Ensure a new session can still be started.
  {
    StartSessionAndConnectBackend();
    RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                         "session_2");
    EXPECT_EQ(session_controller()->GetSessionLifecycle(),
              SessionLifecycle::kLive);
  }
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest,
                       ServerErrorEndsConnectedSession) {
  StartSessionAndConnectBackend();
  RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                       "session_1");
  ASSERT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kLive);
  RespondServerError(
      proto::ServerErrorNotification::ERROR_CODE_INTERNAL_BACKEND_ERROR,
      "internal backend error");

  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kFinished);

  // An error frame doesn't itself drop the connection; that happens when the
  // session is torn down.
  EXPECT_TRUE(backend()->is_transport_connected());

  // The session itself is torn down asynchronously.
  EXPECT_TRUE(ttc_service().is_session_active());
  ASSERT_TRUE(WaitForServiceState(ServiceState::kSessionInactive));
  EXPECT_EQ(session_controller(), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest, ClosedTransportEndsSession) {
  StartSessionAndConnectBackend();
  RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                       "session_1");
  ASSERT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kLive);

  CloseBackendConnection();

  EXPECT_FALSE(backend()->is_transport_connected());
  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kFinished);

  EXPECT_TRUE(ttc_service().is_session_active());
  ASSERT_TRUE(WaitForServiceState(ServiceState::kSessionInactive));
  EXPECT_EQ(session_controller(), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest,
                       ClosedTransportBeforeSetupResponseEndsSession) {
  StartSessionAndConnectBackend();
  ASSERT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kInitializing);

  // The transport drops before the server responds to the setup request.
  CloseBackendConnection();

  EXPECT_FALSE(backend()->is_transport_connected());
  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kFinished);

  EXPECT_TRUE(ttc_service().is_session_active());
  ASSERT_TRUE(WaitForServiceState(ServiceState::kSessionInactive));
  EXPECT_EQ(session_controller(), nullptr);
}

// The transport reports a kConnecting state before it becomes connected. This
// must not be mistaken for a disconnection, which would end the session.
IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest,
                       ConnectingTransportDoesNotEndSession) {
  ttc_service().StartSession();
  ASSERT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kInitializing);

  BeginBackendConnection();

  EXPECT_FALSE(backend()->is_transport_connected());
  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kInitializing);

  // Sessions are torn down asynchronously so drain the task queue to ensure
  // the session survived the connecting notification.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ttc_service().is_session_active());
  ASSERT_NE(session_controller(), nullptr);

  // The session becomes live once the transport connects and the server
  // responds to the setup request.
  OpenBackendConnection();
  RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                       "session_1");

  EXPECT_TRUE(backend()->is_transport_connected());
  EXPECT_EQ(session_controller()->GetSessionLifecycle(),
            SessionLifecycle::kLive);
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest,
                       SessionStatusPropagatesServerSessionIdToActorTask) {
  StartSessionAndConnectBackend();
  RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                       "server_session_42");

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile());
  ASSERT_NE(actor_service, nullptr);
  const actor::ActorTask* task =
      actor_service->GetTask(session_controller()->GetActorTaskId());
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->source_info().type, actor::TaskSourceInfo::Client::kTtc);
  EXPECT_EQ(task->source_info().id, "server_session_42");
}

IN_PROC_BROWSER_TEST_F(TtcProtocolBrowserTest,
                       ToolNavigationRecordsActorVisit) {
  StartSessionAndConnectBackend();
  RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                       "server_session_42");

  const GURL target_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  HistoryVisitWaiter visit_waiter(
      HistoryServiceFactory::GetForProfile(profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS),
      target_url);

  proto::TtcServerFrame tool_frame;
  proto::ToolCall* tool_call = tool_frame.mutable_tool_call();
  tool_call->set_call_id("call_1");
  tool_call->set_name(std::string(actor::OpenUrlToolRequest::kModelFacingName));
  tool_call->set_arguments_json(*base::WriteJson(
      base::DictValue()
          .Set(actor::OpenUrlToolRequest::kUrlParam, target_url.spec())
          .Set(actor::OpenUrlToolRequest::kNewTabParam, false)));
  SimulateResponse(tool_frame);

  const history::VisitedURLInfo visit = visit_waiter.Wait();
  EXPECT_EQ(visit.visit_row.source, history::SOURCE_ACTOR);
  EXPECT_FALSE(visit.visit_row.consider_for_ntp_most_visited);
}

}  // namespace ttc
