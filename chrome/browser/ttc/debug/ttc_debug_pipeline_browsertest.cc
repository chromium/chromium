// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ttc/app/app_browser_test_base.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/debug/ttc_debug_session.h"
#include "chrome/browser/ttc/debug/ttc_debug_tools_service.h"
#include "chrome/browser/ttc/debug/ttc_session_audio_recorder.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/optimization_guide/proto/features/streaming/ttc.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/audio_parameters.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ttc {
namespace {

using Stream = TtcSessionAudioRecorder::Stream;

// Exercises the whole stack: the TTC session, conversation and MES client
// report into the debug record while the debug tools are enabled.
class TtcDebugPipelineBrowserTest : public AppBrowserTestBase {
 public:
  TtcDebugPipelineBrowserTest() {
    feature_list_.InitAndEnableFeature(kTtcDebugTools);
  }

 protected:
  TtcDebugToolsService& debug_tools() {
    return *TtcDebugToolsService::Get(profile());
  }

  // Delivers captured mic audio to the conversation as the AudioController
  // would.
  void CaptureAudio(const std::vector<int16_t>& pcm, float energy) {
    static_cast<AudioController::Observer*>(conversation())
        ->OnCapturedAudio(
            pcm,
            media::AudioParameters(
                media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::ChannelLayoutConfig::Mono(), 16000, pcm.size()),
            energy);
  }

  void RespondToolCall(const std::string& call_id,
                       const std::string& name,
                       const std::string& arguments_json) {
    optimization_guide::proto::TtcServerFrame frame;
    auto* tool_call = frame.mutable_tool_call();
    tool_call->set_call_id(call_id);
    tool_call->set_name(name);
    tool_call->set_arguments_json(arguments_json);
    SimulateResponse(frame);
  }

  static const TtcDebugSession::Issue* FindOpenIssue(TtcDebugSession& session,
                                                     std::string_view key) {
    for (const auto& issue : session.issues()) {
      if (issue.key == key && !issue.resolved) {
        return &issue;
      }
    }
    return nullptr;
  }

  // The samples of the only recorded segment of `stream`.
  static std::vector<int16_t> RecordedSamples(TtcDebugSession& session,
                                              Stream stream) {
    const std::vector<TtcSessionAudioRecorder::Placement> ranges =
        session.recorder().SegmentRanges(stream);
    if (ranges.size() != 1u) {
      return {};
    }
    return session.recorder().GetRange(stream, ranges[0].start, ranges[0].end);
  }

  // Runs until the only tool call has finished.
  [[nodiscard]] static bool WaitForToolFinished(TtcDebugSession& session) {
    return base::test::RunUntil([&] {
      return session.tool_calls().size() == 1u &&
             session.tool_calls()[0].finished_ms.has_value();
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TtcDebugPipelineBrowserTest, RecordsRealPipeline) {
  StartSessionAndConnectBackend();
  TtcDebugSession* session = debug_tools().current_session();
  ASSERT_TRUE(session);
  // The MES client and the audio controller report straight into the record.
  EXPECT_EQ(session_controller()->GetDebugSink(), session);

  RespondSessionStatus(
      optimization_guide::proto::SessionStatusResponse::STATE_CONNECTED,
      "session_dbg");
  EXPECT_EQ(session->GetSnapshot().server_session_id, "session_dbg");
  EXPECT_TRUE(session->GetSnapshot().transport_connected);

  const std::vector<int16_t> mic = {10, -20, 30, -40};
  CaptureAudio(mic, 0.2f);
  EXPECT_EQ(RecordedSamples(*session, Stream::kInput), mic);
  EXPECT_EQ(session->GetSnapshot().chunks_sent, 1);

  const std::vector<int16_t> reply = {7, 8, 9};
  SendServerAudio(reply, /*sequence_number=*/1);
  EXPECT_EQ(RecordedSamples(*session, Stream::kOutput), reply);
  EXPECT_EQ(session->GetSnapshot().audio_chunks_received, 1);

  ttc_service().EndSession();
  // The record outlives the TTC session for review.
  ASSERT_EQ(debug_tools().current_session(), session);
  EXPECT_TRUE(session->ended());
}

// An open_url tool call from the server is traced through the actor to its
// effect on the tab and the response sent back.
IN_PROC_BROWSER_TEST_F(TtcDebugPipelineBrowserTest, TracesToolCallToItsEffect) {
  StartSessionAndConnectBackend();
  TtcDebugSession* session = debug_tools().current_session();
  ASSERT_TRUE(session);
  RespondSessionStatus(
      optimization_guide::proto::SessionStatusResponse::STATE_CONNECTED,
      "session_dbg");

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  RespondToolCall(
      "call-1", "open_url",
      base::StrCat({R"({"url":")", url.spec(), R"(","new_tab":false})"}));

  ASSERT_TRUE(base::test::RunUntil([&] {
    return session->tool_calls().size() == 1u &&
           session->tool_calls()[0].response_sent_ms.has_value() &&
           session->tool_calls()[0].effect !=
               TtcDebugSession::ToolEffect::kChecking;
  }));
  const TtcDebugSession::ToolCall& tool_call = session->tool_calls()[0];
  EXPECT_EQ(tool_call.call_id, "call-1");
  EXPECT_EQ(tool_call.status, TtcDebugSession::ToolStatus::kOk);
  EXPECT_EQ(tool_call.result_code, "kOk");
  ASSERT_TRUE(tool_call.target_before.has_value());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url);
  EXPECT_EQ(tool_call.effect, TtcDebugSession::ToolEffect::kVerified)
      << tool_call.effect_detail;
  EXPECT_FALSE(tool_call.response_json.empty());
  EXPECT_EQ(session->GetSnapshot().open_errors, 0);
}

IN_PROC_BROWSER_TEST_F(TtcDebugPipelineBrowserTest, FailedToolCallIsAnIssue) {
  StartSessionAndConnectBackend();
  TtcDebugSession* session = debug_tools().current_session();
  ASSERT_TRUE(session);

  // Missing the required `url` argument.
  RespondToolCall("call-1", "open_url", "{}");
  ASSERT_TRUE(base::test::RunUntil([&] {
    return session->tool_calls().size() == 1u &&
           session->tool_calls()[0].status ==
               TtcDebugSession::ToolStatus::kError;
  }));
  EXPECT_EQ(session->tool_calls()[0].result_code, "kArgumentsInvalid");
  EXPECT_TRUE(FindOpenIssue(*session, "tool_error:1"));
  EXPECT_GE(session->GetSnapshot().open_errors, 1);
}

// The MES client answers a tool call it can't parse with an error, without
// passing it on.
IN_PROC_BROWSER_TEST_F(TtcDebugPipelineBrowserTest,
                       MalformedToolArgumentsAreRecorded) {
  StartSessionAndConnectBackend();
  TtcDebugSession* session = debug_tools().current_session();
  ASSERT_TRUE(session);

  RespondToolCall("call-1", "navigate", "{not json");
  ASSERT_TRUE(base::test::RunUntil([&] {
    return session->tool_calls().size() == 1u &&
           session->tool_calls()[0].response_sent_ms.has_value();
  }));
  const TtcDebugSession::ToolCall& tool_call = session->tool_calls()[0];
  EXPECT_EQ(tool_call.call_id, "call-1");
  EXPECT_EQ(tool_call.arguments_json, "{not json");
  EXPECT_THAT(tool_call.response_json,
              testing::HasSubstr("Invalid JSON provided for tool arguments"));
}

IN_PROC_BROWSER_TEST_F(TtcDebugPipelineBrowserTest, ServerErrorsAreIssues) {
  StartSessionAndConnectBackend();
  TtcDebugSession* session = debug_tools().current_session();
  ASSERT_TRUE(session);

  RespondServerError(optimization_guide::proto::ServerErrorNotification::
                         ERROR_CODE_INTERNAL_BACKEND_ERROR,
                     "backend exploded");
  const TtcDebugSession::Issue* issue =
      FindOpenIssue(*session, "server_error:ERROR_CODE_INTERNAL_BACKEND_ERROR");
  ASSERT_TRUE(issue);
  EXPECT_EQ(issue->detail, "backend exploded");
}

// Browser-level tools act on the browser rather than the voice-focused tab, so
// they're traced without a target, and their effect isn't checked.
class TtcDebugPipelineBrowserLevelToolTest
    : public TtcDebugPipelineBrowserTest,
      public testing::WithParamInterface<std::string_view> {};

IN_PROC_BROWSER_TEST_P(TtcDebugPipelineBrowserLevelToolTest,
                       TracedWithoutTarget) {
  // The page to switch to, "Title Of Awesomeness", in a background tab: both
  // tools fail if the matching tab is already the active one.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("example.com", "/title2.html")));
  tabs::TabInterface* matching_tab =
      tabs::TabInterface::GetFromContents(web_contents());
  TabListInterface* tab_list =
      TabListInterface::From(matching_tab->GetBrowserWindowInterface());
  tabs::TabInterface* other_tab = tab_list->OpenTab(
      embedded_test_server()->GetURL("example.com", "/title1.html"),
      /*index=*/-1, /*foreground=*/true);
  ASSERT_TRUE(other_tab);
  ASSERT_TRUE(content::WaitForLoadStop(other_tab->GetContents()));
  ASSERT_TRUE(
      base::test::RunUntil([other_tab] { return other_tab->IsActivated(); }));

  StartSessionAndConnectBackend();
  TtcDebugSession* session = debug_tools().current_session();
  ASSERT_TRUE(session);

  RespondToolCall("call-1", std::string(GetParam()),
                  R"({"query":"Awesomeness"})");
  ASSERT_TRUE(WaitForToolFinished(*session));
  ASSERT_EQ(session->tool_calls().size(), 1u);
  const TtcDebugSession::ToolCall& tool_call = session->tool_calls()[0];
  EXPECT_EQ(tool_call.status, TtcDebugSession::ToolStatus::kOk)
      << tool_call.result_code << ": " << tool_call.result_message;
  EXPECT_EQ(tool_call.result_code, "kOk");
  EXPECT_TRUE(tool_call.dispatched_ms.has_value());
  EXPECT_FALSE(tool_call.target_before.has_value());
  EXPECT_EQ(tool_call.effect, TtcDebugSession::ToolEffect::kNotChecked);
  EXPECT_FALSE(FindOpenIssue(*session, "tool_background_target:1"));
  EXPECT_TRUE(matching_tab->IsActivated());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TtcDebugPipelineBrowserLevelToolTest,
    testing::Values("switch_tab", "open_known_page"),
    [](const testing::TestParamInfo<std::string_view>& info) {
      return std::string(info.param);
    });

class TtcDebugPipelineFlagOffBrowserTest : public AppBrowserTestBase {
 public:
  TtcDebugPipelineFlagOffBrowserTest() {
    feature_list_.InitAndDisableFeature(kTtcDebugTools);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Without the flag nothing is recorded, and TTC works as usual.
IN_PROC_BROWSER_TEST_F(TtcDebugPipelineFlagOffBrowserTest, NothingRecorded) {
  EXPECT_FALSE(TtcDebugToolsService::Get(profile()));
  StartSessionAndConnectBackend();
  // Nothing is created for the debug tools, so there is no sink to report
  // into.
  ASSERT_TRUE(session_controller());
  EXPECT_FALSE(session_controller()->GetDebugSink());
  ASSERT_TRUE(conversation());
  RespondSessionStatus(
      optimization_guide::proto::SessionStatusResponse::STATE_CONNECTED,
      "session_dbg");
  EXPECT_FALSE(TtcDebugToolsService::Get(profile()));
}

// The debug tools need TTC itself: with only the debug tools flag on, the
// service doesn't exist.
class TtcDebugPipelineTtcOffBrowserTest : public PlatformBrowserTest {
 public:
  TtcDebugPipelineTtcOffBrowserTest() {
    feature_list_.InitWithFeatures({kTtcDebugTools}, {kTtc});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TtcDebugPipelineTtcOffBrowserTest, NoDebugTools) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  EXPECT_FALSE(TtcDebugToolsService::Get(profile));
}

}  // namespace
}  // namespace ttc
