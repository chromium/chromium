// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_tool_call_tracer.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ttc/debug/ttc_debug_session.h"
#include "chrome/browser/ttc/debug/ttc_debug_tools_service.h"
#include "chrome/browser/ttc/debug/ttc_tool_effect_verifier.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "components/ttc/app/public/tool_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {
namespace {

using testing::Contains;
using testing::Not;
using ToolEffect = TtcDebugSession::ToolEffect;
using ToolStatus = TtcDebugSession::ToolStatus;

constexpr char kUrl[] = "https://a.test/";

// Arguments of an open_url call that loads kUrl in the target tab.
base::DictValue OpenUrlArguments() {
  base::DictValue arguments;
  arguments.Set("url", kUrl);
  arguments.Set("new_tab", false);
  return arguments;
}

class TtcToolCallTracerTest : public ChromeRenderViewHostTestHarness {
 protected:
  TtcToolCallTracerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    service_ = std::make_unique<TtcDebugToolsService>();
    service_->SetTickClockForTesting(task_environment()->GetMockTickClock());
    session_ = service_->BeginSession(/*journal=*/nullptr);
    tracer_ = std::make_unique<TtcToolCallTracer>(
        *session_, actor_journal_,
        base::BindRepeating(&TtcToolCallTracerTest::GetTaskId,
                            base::Unretained(this)));
  }

  void TearDown() override {
    tracer_.reset();
    session_ = nullptr;
    service_->Shutdown();
    service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  actor::TaskId GetTaskId() const { return task_id_; }

  TtcDebugSession& session() { return *session_; }

  // The keys of the problems the session has raised.
  std::vector<std::string> IssueKeys() {
    std::vector<std::string> keys;
    for (const TtcDebugSession::Issue& issue : session().issues()) {
      keys.push_back(issue.key);
    }
    return keys;
  }

  const TtcDebugSession::ToolCall& tool_call() {
    EXPECT_EQ(session().tool_calls().size(), 1u);
    return session().tool_calls()[0];
  }

  // Receives and dispatches an open_url call to web_contents(), whose effect
  // is then checked.
  void DispatchOpenUrl() {
    session().OnServerFrame(DebugSink::ToolCall{
        .call_id = "call-1",
        .name = "open_url",
        .arguments_json = R"({"url":"https://a.test/","new_tab":false})"});
    tracer_->OnToolDispatched("call-1", "open_url", OpenUrlArguments());
    tracer_->OnToolTarget("call-1", *web_contents());
    ASSERT_EQ(tool_call().effect, ToolEffect::kChecking);
  }

  void CommitNavigation() {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               GURL(kUrl));
  }

  // Logs an actor journal entry on `track_uuid` for `task`.
  void LogActorEntry(actor::TaskId task,
                     uint64_t track_uuid,
                     std::string_view event) {
    actor_journal_.Log(GURL(kUrl), task, track_uuid, event,
                       actor::JournalDetailsBuilder().Add("k", "v").Build());
    // The tracer handles entries asynchronously.
    task_environment()->RunUntilIdle();
  }

  bool HasActorEvent(std::string_view event) {
    for (const TtcDebugSession::TimelineEvent& timeline_event :
         session().events()) {
      if (timeline_event.kind == TtcDebugSession::EventKind::kActor &&
          timeline_event.label.starts_with(event)) {
        return true;
      }
    }
    return false;
  }

  actor::AggregatedJournal actor_journal_;
  actor::TaskId task_id_{7};
  std::unique_ptr<TtcDebugToolsService> service_;
  raw_ptr<TtcDebugSession> session_;
  std::unique_ptr<TtcToolCallTracer> tracer_;
};

// A successful tool reports its result before its effect lands.
TEST_F(TtcToolCallTracerTest, ToolFinishedBeforeEffect) {
  DispatchOpenUrl();

  tracer_->OnToolFinished("call-1", ToolResponse::Success());
  EXPECT_EQ(tool_call().status, ToolStatus::kOk);
  EXPECT_EQ(tool_call().result_code, "kOk");
  EXPECT_EQ(tool_call().effect, ToolEffect::kChecking);

  CommitNavigation();
  EXPECT_EQ(tool_call().effect, ToolEffect::kVerified)
      << tool_call().effect_detail;
}

// The effect often lands before the actor reports its result; the target is
// kept until then for the after-snapshot.
TEST_F(TtcToolCallTracerTest, EffectBeforeToolFinished) {
  DispatchOpenUrl();

  CommitNavigation();
  EXPECT_EQ(tool_call().effect, ToolEffect::kVerified)
      << tool_call().effect_detail;

  tracer_->OnToolFinished("call-1", ToolResponse::Success());
  EXPECT_EQ(tool_call().status, ToolStatus::kOk);
  ASSERT_TRUE(tool_call().target_after.has_value());
  EXPECT_EQ(tool_call().target_after->url, kUrl);
}

// A tool that reports success but changes nothing: the case the effect check
// exists for.
TEST_F(TtcToolCallTracerTest, SuccessWithoutEffectIsAnIssue) {
  DispatchOpenUrl();

  tracer_->OnToolFinished("call-1", ToolResponse::Success());
  EXPECT_EQ(tool_call().status, ToolStatus::kOk);
  EXPECT_EQ(tool_call().effect, ToolEffect::kChecking);

  task_environment()->FastForwardBy(TtcToolEffectVerifier::kTimeout);
  EXPECT_EQ(tool_call().status, ToolStatus::kOk);
  EXPECT_EQ(tool_call().effect, ToolEffect::kNoEffect);
  EXPECT_THAT(IssueKeys(), Contains("tool_no_effect:1"));
}

// A failed tool has no effect to check: its verifier is dropped.
TEST_F(TtcToolCallTracerTest, FailedToolStopsChecking) {
  DispatchOpenUrl();

  tracer_->OnToolFinished(
      "call-1", ToolResponse::Error(
                    actor::mojom::ActionResultCode::kTabWentAway, "gone"));
  EXPECT_EQ(tool_call().status, ToolStatus::kError);
  EXPECT_EQ(tool_call().result_code, "kTabWentAway");
  EXPECT_EQ(tool_call().result_message, "gone");

  CommitNavigation();
  task_environment()->FastForwardBy(TtcToolEffectVerifier::kTimeout);
  EXPECT_EQ(tool_call().effect, ToolEffect::kNotChecked);
  EXPECT_THAT(IssueKeys(), Not(Contains("tool_no_effect:1")));
}

// A navigation that commits an error page reports its missing effect before
// the tool reports the error. The error explains it.
TEST_F(TtcToolCallTracerTest, EffectFailureBeforeToolError) {
  DispatchOpenUrl();
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kUrl), net::ERR_CONNECTION_REFUSED);
  ASSERT_EQ(tool_call().effect, ToolEffect::kNoEffect);

  tracer_->OnToolFinished(
      "call-1",
      ToolResponse::Error(
          actor::mojom::ActionResultCode::kNavigateCommittedErrorPage));
  EXPECT_EQ(tool_call().status, ToolStatus::kError);
  EXPECT_EQ(tool_call().effect, ToolEffect::kNotChecked);
  const TtcDebugSession::Issue* no_effect = nullptr;
  for (const TtcDebugSession::Issue& issue : session().issues()) {
    if (issue.key == "tool_no_effect:1") {
      no_effect = &issue;
    }
  }
  ASSERT_TRUE(no_effect);
  EXPECT_TRUE(no_effect->resolved);
}

TEST_F(TtcToolCallTracerTest, FailureWithoutCodeIsAnError) {
  session().OnServerFrame(DebugSink::ToolCall{
      .call_id = "call-1", .name = "open_url", .arguments_json = "{}"});
  tracer_->OnToolDispatched("call-1", "open_url", base::DictValue());
  tracer_->OnToolFinished("call-1", ToolResponse::Error("failed"));
  EXPECT_EQ(tool_call().status, ToolStatus::kError);
  EXPECT_EQ(tool_call().result_code, "kError");
}

// The tab a tool acts on is recorded when the tool reports it, whichever tool
// it is: nothing here knows the tool by name.
TEST_F(TtcToolCallTracerTest, AnyToolRecordsTheTargetItReports) {
  session().OnServerFrame(DebugSink::ToolCall{
      .call_id = "call-1", .name = "a_new_tool", .arguments_json = "{}"});
  tracer_->OnToolDispatched("call-1", "a_new_tool", base::DictValue());
  EXPECT_EQ(tool_call().status, ToolStatus::kRunning);
  EXPECT_FALSE(tool_call().target_before.has_value());

  CommitNavigation();
  tracer_->OnToolTarget("call-1", *web_contents());
  ASSERT_TRUE(tool_call().target_before.has_value());
  EXPECT_EQ(tool_call().target_before->url, kUrl);
  // Nothing is known about the effect a_new_tool should have.
  EXPECT_EQ(tool_call().effect, ToolEffect::kNotChecked);

  tracer_->OnToolFinished("call-1", ToolResponse::Success());
  ASSERT_TRUE(tool_call().target_after.has_value());
  EXPECT_EQ(tool_call().target_after->url, kUrl);
}

// Browser-level tools report no target, so their effect isn't checked.
TEST_F(TtcToolCallTracerTest, ToolWithoutTargetIsNotChecked) {
  session().OnServerFrame(
      DebugSink::ToolCall{.call_id = "call-1",
                          .name = "open_known_page",
                          .arguments_json = R"({"query":"a"})"});
  base::DictValue arguments;
  arguments.Set("query", "a");
  tracer_->OnToolDispatched("call-1", "open_known_page", arguments);
  EXPECT_TRUE(tool_call().dispatched_ms.has_value());
  EXPECT_FALSE(tool_call().target_before.has_value());
  EXPECT_EQ(tool_call().effect, ToolEffect::kNotChecked);

  tracer_->OnToolFinished("call-1", ToolResponse::Success());
  EXPECT_EQ(tool_call().status, ToolStatus::kOk);
  EXPECT_EQ(tool_call().effect, ToolEffect::kNotChecked);
}

// Only what the actor framework logs for the session's own task is recorded.
TEST_F(TtcToolCallTracerTest, RecordsOnlyTheActorFrameworksTracks) {
  const actor::TaskId other_task(8);
  LogActorEntry(task_id_, actor::MakeBrowserTrackUUID(task_id_), "Browser");
  LogActorEntry(task_id_, actor::MakeRendererTrackUUID(task_id_), "Renderer");
  LogActorEntry(other_task, actor::MakeBrowserTrackUUID(other_task),
                "OtherTask");
  // What TTC logs is recorded directly, not from the actor journal.
  LogActorEntry(task_id_, actor::MakeFrontEndTrackUUID(task_id_), "FrontEnd");
  LogActorEntry(task_id_, actor::MakeTtcBackendTrackUUID(task_id_), "Backend");
  LogActorEntry(task_id_, actor::MakeTtcCurrentTurnTrackUUID(task_id_),
                "CurrentTurn");
  LogActorEntry(task_id_, actor::MakeTtcMesClientRecvTrackUUID(task_id_),
                "MesClientRecv");
  LogActorEntry(task_id_, actor::MakeTtcMesClientSendTrackUUID(task_id_),
                "MesClientSend");
  // Nor is a track nothing here knows about, such as one TTC adds later.
  LogActorEntry(task_id_, /*track_uuid=*/12345, "NewTrack");

  EXPECT_TRUE(HasActorEvent("Browser"));
  EXPECT_TRUE(HasActorEvent("Renderer"));
  EXPECT_FALSE(HasActorEvent("OtherTask"));
  EXPECT_FALSE(HasActorEvent("FrontEnd"));
  EXPECT_FALSE(HasActorEvent("Backend"));
  EXPECT_FALSE(HasActorEvent("CurrentTurn"));
  EXPECT_FALSE(HasActorEvent("MesClientRecv"));
  EXPECT_FALSE(HasActorEvent("MesClientSend"));
  EXPECT_FALSE(HasActorEvent("NewTrack"));
}

TEST_F(TtcToolCallTracerTest, IgnoresActorJournalWithoutTask) {
  task_id_ = actor::TaskId();
  LogActorEntry(actor::TaskId(), actor::MakeBrowserTrackUUID(actor::TaskId()),
                "NoTask");
  EXPECT_FALSE(HasActorEvent("NoTask"));
}

// The tracer goes away with the TTC session, possibly while a tool's effect
// is still being checked. The record keeps what it got, and nothing reaches it
// afterwards.
TEST_F(TtcToolCallTracerTest, StopsWithTheSession) {
  DispatchOpenUrl();
  tracer_.reset();

  CommitNavigation();
  task_environment()->FastForwardBy(TtcToolEffectVerifier::kTimeout);
  LogActorEntry(task_id_, actor::MakeBrowserTrackUUID(task_id_), "Late");
  EXPECT_TRUE(tool_call().target_before.has_value());
  EXPECT_EQ(tool_call().effect, ToolEffect::kChecking);
  EXPECT_FALSE(HasActorEvent("Late"));
}

}  // namespace
}  // namespace ttc
