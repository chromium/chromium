// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/script_tool_host.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

using base::test::TestFuture;

namespace actor {

class ActorToolsTestScriptTool : public ActorToolsTest {
 public:
  ActorToolsTestScriptTool() {
    features_.InitWithFeatures(
        {blink::features::kWebMCP, blink::features::kDevToolsWebMCPSupport,
         actor::kGlicActorEnableScriptTools,
         actor::kActorScriptToolTransientUserActivation},
        {});
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  struct ToolResult {
    mojom::ActionResultPtr action_result;
    mojom::ScriptToolResponsePtr response;
  };

  ToolResult RunScriptTool(std::unique_ptr<ToolRequest> action) {
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
    ExpectOkResult(result);

    const auto& action_results = result.Get();
    EXPECT_EQ(action_results.size(), 1u);
    EXPECT_TRUE(action_results.at(0).result);
    mojom::ActionResultPtr action_result = action_results.at(0).result->Clone();
    actor::mojom::ScriptToolResponsePtr response =
        std::move(action_results.at(0).result->script_tool_response);
    EXPECT_TRUE(response);
    return {std::move(action_result), std::move(response)};
  }

  void RunScriptToolExpectingError(std::unique_ptr<ToolRequest> action,
                                   mojom::ActionResultCode expected_error) {
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
    ExpectErrorResult(result, expected_error);
  }

  void RunDeclarativeToolExpectingError(
      mojom::ActionResultCode expected_error,
      const std::string& input = R"JSON({"echo": "hello world"})JSON") {
    auto action =
        MakeScriptToolRequest(*main_frame(), "declarative_tool", input);
    RunScriptToolExpectingError(std::move(action), expected_error);
  }

  void RunNavigatingScriptTool(content::RenderFrameHost& rfh,
                               const std::string& name,
                               const std::string& input_arguments) {
    auto action = MakeScriptToolRequest(rfh, name, input_arguments);
    content::TestNavigationObserver nav_observer(web_contents());
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());

    auto response = result.Get();
    ExpectOkResult(*response[0].result);
  }

  content::RenderFrameHost& NavigateSubframe(
      const GURL& main_url,
      const GURL& subframe_url,
      std::optional<std::string_view> allow = std::nullopt,
      std::optional<std::string_view> sandbox = std::nullopt) {
    CHECK(content::NavigateToURL(web_contents(), main_url));
    if (sandbox.has_value()) {
      CHECK(content::ExecJs(
          web_contents(),
          content::JsReplace(
              "document.getElementById('iframe').setAttribute('sandbox', $1);",
              *sandbox)));
    }
    if (allow.has_value()) {
      CHECK(content::ExecJs(
          web_contents(),
          content::JsReplace(
              "document.getElementById('iframe').setAttribute('allow', $1);",
              *allow)));
    }
    CHECK(content::NavigateIframeToURL(web_contents(), "iframe", subframe_url));
    content::RenderFrameHost* subframe = ChildFrameAt(main_frame(), 0);
    CHECK(subframe);
    return *subframe;
  }

 private:
  base::test::ScopedFeatureList features_;
};

class ActorToolsTestScriptToolWithStability : public ActorToolsTestScriptTool {
 public:
  ActorToolsTestScriptToolWithStability() {
    features_.InitAndEnableFeatureWithParameters(
        actor::kActorScriptToolDelayObservation,
        {{"script_tool_delay_observation_ms", "1000"}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptToolWithStability,
                       PageStabilityDelay) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);

  base::TimeTicks start = base::TimeTicks::Now();
  auto [action_result, response] = RunScriptTool(std::move(action));
  base::TimeDelta duration = base::TimeTicks::Now() - start;

  EXPECT_EQ(response->result, "test");
  // The delay should be at least 1000ms.
  EXPECT_GE(duration, base::Milliseconds(1000));
  EXPECT_TRUE(action_result->requires_page_stabilization);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, Basic) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments =
      R"JSON(
        { "text": "This is an example sentence." }
      )JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "This is an example sentence.");
  EXPECT_EQ(response->input_arguments, input_arguments);
  EXPECT_EQ(response->tool->name, "echo");
  EXPECT_EQ(response->tool->description, "echo input");
  EXPECT_EQ(response->tool->annotations->read_only, true);
  EXPECT_FALSE(action_result->requires_page_stabilization);

  const std::string expected_input_schema =
      R"JSON({"type":"object","properties":{"text":{"description":)JSON"
      R"JSON("Value to echo","type":"string"}},"required":["text"]})JSON";
  EXPECT_EQ(response->tool->input_schema, expected_input_schema);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, DeclarativeTool) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string declarative_input =
      R"JSON(
        {
          "text": "text #1",
          "text2": "text #2",
          "select": "Option 2"
        }
      )JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->tool->name, "declarative_tool");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->input_arguments, declarative_input);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigateAfterResponse) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_navigate_after_response.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments =
      R"JSON(
      { "text": "This is an example sentence." }
    )JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "This is an example sentence.");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, DeclarativeToolCrossDocument) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string declarative_input =
      R"JSON(
        {
          "echo": "hello world"
        }
      )JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);

  auto [action_result, response] = RunScriptTool(std::move(action));
  ASSERT_TRUE(response);
  EXPECT_EQ(response->input_arguments, declarative_input);
  EXPECT_EQ(response->tool->name, "declarative_tool");
  EXPECT_EQ(response->tool->description, "A declarative WebMCP tool");
  EXPECT_FALSE(response->tool->annotations);
  const std::string expected_input_schema =
      R"JSON({"type":"object","properties":{"echo":{"type":"string","description":"Value to echo"}},"required":["echo"]})JSON";
  EXPECT_EQ(response->tool->input_schema, expected_input_schema);

  base::Value actual_json = base::test::ParseJson(*response->result);
  base::Value expected_json = base::test::ParseJson(R"JSON(
  [
    {
      "@context": "https://schema.org",
      "@type": "Message",
      "text": "echoed value"
    },
    {
      "@context": "https://schema.org",
      "@type": "Message",
      "text": "extra stuff"
    }
  ]
)JSON");

  EXPECT_EQ(actual_json, expected_json);
}

class ActorToolsTestScriptToolNoTimeout : public ActorToolsTest {
 public:
  ActorToolsTestScriptToolNoTimeout() {
    features_.InitWithFeaturesAndParameters(
        {{blink::features::kWebMCP, {}},
         {blink::features::kDevToolsWebMCPSupport, {}},
         {actor::kGlicActorEnableScriptTools, {{"execution_timeout", "1s"}}}},
        {});
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptToolNoTimeout,
                       DeclarativeToolNoTimeout) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_pause.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string declarative_input =
      R"JSON({"echo":"declarative_input"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  // Wait for the task to be paused. The Act() call has not returned yet.
  if (base::FeatureList::IsEnabled(kActorFormScriptToolInterrupt)) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return actor_task().GetState() == ActorTask::State::kWaitingOnUser;
    }));
  } else {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return actor_task().GetState() == ActorTask::State::kPausedByActor;
    }));
  }

  // Wait for more than the timeout (1s).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1500));
  run_loop.Run();

  // Verify that the task is still paused and has not timed out.
  if (base::FeatureList::IsEnabled(kActorFormScriptToolInterrupt)) {
    EXPECT_EQ(actor_task().GetState(), ActorTask::State::kWaitingOnUser);
  } else {
    EXPECT_EQ(actor_task().GetState(), ActorTask::State::kPausedByActor);
  }

  EXPECT_FALSE(result.IsReady());

  // Trigger the submission manually.
  content::ExecuteScriptAsync(web_contents(),
                              "document.querySelector('button').click()");

  // The Act() call should now complete successfully.
  ExpectOkResult(result);
  EXPECT_EQ(actor_task().GetState(), ActorTask::State::kReflecting);

  const auto& action_results = result.Get();
  ASSERT_EQ(action_results.size(), 1u);
  ASSERT_TRUE(action_results.at(0).result->script_tool_response);
  base::Value actual_json = base::test::ParseJson(
      *action_results.at(0).result->script_tool_response->result);

  base::Value expected_json = base::test::ParseJson(R"JSON(
  [
    {
      "@context": "https://schema.org",
      "@type": "Message",
      "text": "echoed value"
    },
    {
      "@context": "https://schema.org",
      "@type": "Message",
      "text": "extra stuff"
    }
  ]
  )JSON");
  EXPECT_EQ(actual_json, expected_json);

  // Verify that the task can be stopped cleanly.
  actor_task().Stop(ActorTask::StoppedReason::kTaskComplete);
  EXPECT_EQ(actor_keyed_service().GetTask(task_id_), nullptr);
}
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, Histograms) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string valid_input_arguments = R"JSON({"text": "test"})JSON";
  auto action =
      MakeScriptToolRequest(*main_frame(), "echo", valid_input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));

  histogram_tester.ExpectUniqueSample("Actor.Tools.ScriptTool.InputSizeBytes",
                                      valid_input_arguments.size(), 1);
  histogram_tester.ExpectUniqueSample("Actor.Tools.ScriptTool.InvocationResult",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Actor.Tools.ScriptTool.ActionResultCode",
                                      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectUniqueSample("Actor.Tools.ScriptTool.OutputSizeBytes",
                                      std::string("test").size(), 1);

  // Test a failure case.
  const std::string input_arguments = R"JSON({})JSON";
  auto bad_action =
      MakeScriptToolRequest(*main_frame(), "invalid", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(bad_action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolInvalidName);

  histogram_tester.ExpectBucketCount("Actor.Tools.ScriptTool.InputSizeBytes",
                                     input_arguments.size(), 1);
  histogram_tester.ExpectBucketCount("Actor.Tools.ScriptTool.InvocationResult",
                                     false, 1);
  histogram_tester.ExpectBucketCount(
      "Actor.Tools.ScriptTool.ActionResultCode",
      mojom::ActionResultCode::kScriptToolInvalidName, 1);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, HasTransientUserActivation) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    document.modelContext.registerTool({
      execute: async () => {
        let hasUserActivation = navigator.userActivation.isActive;
        return hasUserActivation ? "true" : "false";
      },
      name: 'check_activation',
      description: 'test',
      inputSchema: { type: 'object', properties: {} }
    });
  )",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  auto action = MakeScriptToolRequest(*main_frame(), "check_activation", "{}");
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "true");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, WindowOpenTopLevelNavigate) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "window.name = 'already-existing-top-level';"));

  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    document.modelContext.registerTool({
      execute: async () => {
        let w = window.open('/title1.html', 'already-existing-top-level');
        return w ? "opened" : "blocked";
      },
      name: 'window_open_top',
      description: 'test',
      inputSchema: { type: 'object', properties: {} }
    });
  )",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  content::TestNavigationObserver nav_observer(web_contents());

  auto action = MakeScriptToolRequest(*main_frame(), "window_open_top", "{}");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  nav_observer.Wait();

  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  auto results = result.Get();
  EXPECT_EQ(results[0].result->script_tool_response->result, "opened");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, WindowOpenSucceeds) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    document.modelContext.registerTool({
      execute: async () => {
        let w = window.open('about:blank', '_blank');
        return w ? "opened" : "blocked";
      },
      name: 'window_open',
      description: 'test',
      inputSchema: { type: 'object', properties: {} }
    });
  )",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  auto original_tab_id = active_tab()->GetHandle().raw_value();
  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();

  auto action = MakeScriptToolRequest(*main_frame(), "window_open", "{}");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  nav_observer.Wait();

  auto results = result.Get();
  EXPECT_EQ(results[0].result->script_tool_response->result, "opened");

  // Validate the newly opened window is not included in the observation's tab
  // list, but is included in the window observations.
  base::test::TestFuture<
      base::TimeTicks, std::vector<actor::ActionResultWithLatencyInfo>,
      actor::TaskId, bool,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>,
      std::unique_ptr<optimization_guide::proto::ActionsResult>,
      std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>>
      future;

  actor::BuildActionsResultWithObservations(
      *browser()->GetProfile(), /*start_time=*/base::TimeTicks::Now(), results,
      actor_task(), /*skip_async_observation_information=*/true,
      /*screenshot_collection_options=*/std::nullopt, future.GetCallback());

  const std::unique_ptr<optimization_guide::proto::ActionsResult>&
      actions_result = future.Get<5>();
  ASSERT_TRUE(actions_result);

  // window.open opens a new tab in the same browser window.
  // We expect 1 window with 2 tabs in the window observations.
  EXPECT_EQ(actions_result->windows_size(), 1);
  EXPECT_EQ(actions_result->windows(0).tab_ids_size(), 2);

  // However, only the original tab should be in the detailed tab observations.
  EXPECT_EQ(actions_result->tabs_size(), 1);
  EXPECT_EQ(actions_result->tabs(0).id(), original_tab_id);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       WindowOpenSecondAttemptBlocked) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    document.modelContext.registerTool({
      execute: async () => {
        let w1 = window.open('about:blank', '_blank');
        let w2 = window.open('about:blank', '_blank');
        return JSON.stringify({
          first: w1 ? "opened" : "blocked",
          second: w2 ? "opened" : "blocked"
        });
      },
      name: 'window_open_twice',
      description: 'test',
      inputSchema: { type: 'object', properties: {} }
    });
  )",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();

  auto action = MakeScriptToolRequest(*main_frame(), "window_open_twice", "{}");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  nav_observer.Wait();

  auto results = result.Get();
  EXPECT_EQ(results[0].result->script_tool_response->result,
            R"({"first":"opened","second":"blocked"})");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigationFailed) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Insert a throttle to cancel the navigation.
  content::TestNavigationThrottleInserter throttle_inserter(
      web_contents(),
      base::BindLambdaForTesting(
          [&](content::NavigationThrottleRegistry& registry) -> void {
            auto throttle =
                std::make_unique<content::TestNavigationThrottle>(registry);
            throttle->SetResponse(
                content::TestNavigationThrottle::WILL_START_REQUEST,
                content::TestNavigationThrottle::SYNCHRONOUS,
                content::NavigationThrottle::CANCEL);
            registry.AddThrottle(std::move(throttle));
          }));

  RunDeclarativeToolExpectingError(
      mojom::ActionResultCode::kScriptToolNavigationDidNotCommit);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigationCommittedErrorPage) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Change form action to a non-existent path on the same server, which should
  // result in an error page (404).
  const GURL error_url =
      embedded_https_test_server().GetURL("example.com", "/non-existent");
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("document.querySelector('form').action = $1",
                         error_url)));

  RunDeclarativeToolExpectingError(
      mojom::ActionResultCode::kScriptToolNavigationCommittedErrorPage);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigationFailedLoad) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL fail_url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_cross_document_fail.html");
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("document.querySelector('form').action = $1",
                         fail_url)));

  RunDeclarativeToolExpectingError(
      mojom::ActionResultCode::kScriptToolNavigationFailedLoad);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigationBlockedByCSP) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Inject a CSP meta tag to block form submission.
  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    var meta = document.createElement('meta');
    meta.httpEquiv = 'Content-Security-Policy';
    meta.content = "form-action 'none'";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )"));

  RunDeclarativeToolExpectingError(
      mojom::ActionResultCode::kScriptToolNavigationDidNotCommit);
}
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       OtherFrameNavigationDoesNotCancelTool) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_slow.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "let f = document.createElement('iframe'); "
                              "document.body.appendChild(f);"));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  content::TestNavigationObserver nav_observer(web_contents());
  content::ExecuteScriptAsync(
      web_contents(), "document.querySelector('iframe').src = '/title1.html';");
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  auto response = result.Get();
  ExpectOkResult(*response[0].result);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       SameDocumentNavigationDoesNotCancelTool) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_slow.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  content::TestNavigationObserver nav_observer(web_contents());
  content::ExecuteScriptAsync(web_contents(),
                              "window.location.hash = \"#test\";");
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  auto response = result.Get();
  ExpectOkResult(*response[0].result);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       UnrelatedNavigationCancelsTool) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_slow.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return actor_task().GetState() == ActorTask::State::kActing; }));

  // Trigger a browser-initiated navigation.
  content::TestNavigationObserver nav_observer(web_contents());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  // The tool should be cancelled because it was replaced by an unrelated
  // navigation.
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolCancelled);
}

// TODO(crbug.com/529908643): Fails due to dangling raw_ptr crash on ChromeOS.
IN_PROC_BROWSER_TEST_F(
    ActorToolsTestScriptTool,
    DISABLED_TabClosedWhileWaitingForNavigationDoesNotCrash) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_slow.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    document.modelContext.registerTool({
      execute: async () => {
        window.location.href = '/title1.html';
        return new Promise(r => {});
      },
      name: 'navigate_and_hang',
      description: 'test',
      inputSchema: { type: 'object', properties: {} }
    });
  )"));

  auto action = MakeScriptToolRequest(*main_frame(), "navigate_and_hang", "{}");
  ActResultFuture result;
  content::TestNavigationManager nav_manager(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html"));
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  auto act_result = result.Get();
  EXPECT_FALSE(act_result.empty());
}
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       BrowserInitiatedBackNavigationFailsTool) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_slow.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return actor_task().GetState() == ActorTask::State::kActing; }));

  content::TestNavigationObserver nav_observer(web_contents());
  web_contents()->GetController().GoBack();
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolCancelled);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, ToolSelfNavigates) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_self_navigate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  RunNavigatingScriptTool(*main_frame(), "navigate",
                          R"JSON({"text": "test_input"})JSON");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, ToolNavigatesAsyncTask) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_self_navigate_delayed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  RunNavigatingScriptTool(*main_frame(), "navigate_delayed",
                          R"JSON({"text": "test_input"})JSON");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, SameOriginSubframeInDocument) {
  content::RenderFrameHost& subframe =
      NavigateSubframe(embedded_https_test_server().GetURL(
                           "example.com", "/actor/simple_iframe.html"),
                       embedded_https_test_server().GetURL(
                           "example.com", "/actor/script_tool.html"));
  ASSERT_FALSE(subframe.IsCrossProcessSubframe());

  const std::string input_arguments =
      R"JSON({"text": "same_origin_subframe"})JSON";
  auto action = MakeScriptToolRequest(subframe, "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "same_origin_subframe");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, SameOriginSubframeNavigates) {
  content::RenderFrameHost& subframe = NavigateSubframe(
      embedded_https_test_server().GetURL("example.com",
                                          "/actor/simple_iframe.html"),
      embedded_https_test_server().GetURL(
          "example.com", "/actor/script_tool_self_navigate.html"));
  ASSERT_FALSE(subframe.IsCrossProcessSubframe());

  RunNavigatingScriptTool(subframe, "navigate",
                          R"JSON({"text": "test_input"})JSON");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       CrossOriginSubframeInDocument) {
  content::RenderFrameHost& subframe = NavigateSubframe(
      embedded_https_test_server().GetURL("a.com", "/actor/simple_iframe.html"),
      embedded_https_test_server().GetURL("b.com", "/actor/script_tool.html"),
      /*allow=*/"tools");
  ASSERT_TRUE(subframe.IsCrossProcessSubframe());

  const std::string input_arguments =
      R"JSON({"text": "cross_origin_subframe"})JSON";
  auto action = MakeScriptToolRequest(subframe, "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "cross_origin_subframe");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, CrossOriginSubframeNavigates) {
  content::RenderFrameHost& subframe = NavigateSubframe(
      embedded_https_test_server().GetURL("a.com", "/actor/simple_iframe.html"),
      embedded_https_test_server().GetURL(
          "b.com", "/actor/script_tool_self_navigate.html"),
      /*allow=*/"tools");
  ASSERT_TRUE(subframe.IsCrossProcessSubframe());

  RunNavigatingScriptTool(subframe, "navigate",
                          R"JSON({"text": "test_input"})JSON");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       OpaqueOriginSubframeInDocument) {
  content::RenderFrameHost& subframe =
      NavigateSubframe(embedded_https_test_server().GetURL(
                           "example.com", "/actor/simple_iframe.html"),
                       embedded_https_test_server().GetURL(
                           "example.com", "/actor/script_tool.html"),
                       /*allow=*/"tools",
                       /*sandbox=*/"allow-scripts");
  EXPECT_TRUE(subframe.GetLastCommittedOrigin().opaque());

  const std::string input_arguments =
      R"JSON({"text": "opaque_origin_subframe"})JSON";
  auto action = MakeScriptToolRequest(subframe, "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "opaque_origin_subframe");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, ToolReentrantExecution) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool_slow.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action1 = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  ActResultFuture result1;
  actor_task().Act(ToRequestList(std::move(action1)), result1.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return actor_task().GetState() == ActorTask::State::kActing; }));

  auto action2 = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  ActResultFuture result2;
  auto task_id2 = actor_keyed_service().CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_keyed_service().GetTask(task_id2)->Act(
      ToRequestList(std::move(action2)), result2.GetCallback());

  auto response1 = result1.Get();
  auto response2 = result2.Get();
  EXPECT_TRUE(response1[0].result);
  EXPECT_TRUE(response2[0].result);
}

IN_PROC_BROWSER_TEST_F(
    ActorToolsTestScriptTool,
    BrowserInitiatedBackNavigationWhileWaitingForUserFailsTool) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/declarative_script_tool_pause.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string declarative_input =
      R"JSON({"echo":"declarative_input"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  if (base::FeatureList::IsEnabled(kActorFormScriptToolInterrupt)) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return actor_task().GetState() == ActorTask::State::kWaitingOnUser;
    }));
  } else {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return actor_task().GetState() == ActorTask::State::kPausedByActor;
    }));
  }

  content::TestNavigationObserver nav_observer(web_contents());
  web_contents()->GetController().GoBack();
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolCancelled);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, DefaultVoting) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectOkResult(result);

  const TabObservationStrategy& strategy = result.GetStrategy();
  EXPECT_EQ(strategy.GetScreenshotPolicy(active_tab()->GetHandle()),
            ScreenshotPolicy::kRequested);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(active_tab()->GetHandle()),
            PageContentExtractionPolicy::kRequired);
}

class ActorToolsTestScriptToolSkipVoting : public ActorToolsTestScriptTool {
 public:
  ActorToolsTestScriptToolSkipVoting() {
    features_.InitWithFeatures({actor::kActorScriptToolSkipScreenshot,
                                actor::kActorScriptToolSkipPageContent},
                               {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptToolSkipVoting, SkipVoting) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectOkResult(result);

  const TabObservationStrategy& strategy = result.GetStrategy();
  EXPECT_EQ(strategy.GetScreenshotPolicy(active_tab()->GetHandle()),
            ScreenshotPolicy::kSkipped);
  EXPECT_EQ(strategy.GetPageContentExtractionPolicy(active_tab()->GetHandle()),
            PageContentExtractionPolicy::kSkipped);
}

}  // namespace actor
