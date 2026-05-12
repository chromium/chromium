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
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/script_tool_host.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

using base::test::TestFuture;

namespace actor {


class ActorToolsTestScriptTool : public ActorToolsTest,
                                 public testing::WithParamInterface<bool> {
 public:
  ActorToolsTestScriptTool() {
    std::vector<base::test::FeatureRef> enabled_features = {
        blink::features::kWebMCP, blink::features::kDevToolsWebMCPSupport,
        actor::kGlicActorEnableScriptTools};
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam()) {
      enabled_features.push_back(::features::kBackForwardCache);
    } else {
      disabled_features.push_back(::features::kBackForwardCache);
    }

    features_.InitWithFeatures(enabled_features, disabled_features);
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptToolWithStability,
                       PageStabilityDelay) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
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

INSTANTIATE_TEST_SUITE_P(All,
                         ActorToolsTestScriptToolWithStability,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "BFCacheEnabled"
                                             : "BFCacheDisabled";
                         });

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, Basic) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, BadToolName) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments =
      R"JSON(
        { "text": "This is an example sentence." }
      )JSON";
  auto action =
      MakeScriptToolRequest(*main_frame(), "invalid", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolInvalidName);
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, DeclarativeTool) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/declarative_script_tool.html");
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, NavigateAfterResponse) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/script_tool_navigate_after_response.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments =
      R"JSON(
      { "text": "This is an example sentence." }
    )JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "This is an example sentence.");
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, DeclarativeToolCrossDocument) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document.html");
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool,
                       DeclarativeToolCrossDocument_No_Autosubmit) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_pause.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string declarative_input =
      R"JSON(
        {
          "echo": "hello world"
        }
      )JSON";
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

  // Trigger the submission manually.
  content::ExecuteScriptAsync(web_contents(),
                              "document.querySelector('button').click()");

  // The Act() call should now complete successfully with the result of the
  // navigation.
  ExpectOkResult(result);
  EXPECT_EQ(actor_task().GetState(), ActorTask::State::kReflecting);

  const auto& action_results = result.Get();
  ASSERT_EQ(action_results.size(), 1u);
  ASSERT_TRUE(action_results.at(0).result->script_tool_response);
  base::Value actual_json = base::test::ParseJson(
      *action_results.at(0).result->script_tool_response->result);

  // The result should match the static content of the page.
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

class ActorToolsTestScriptToolNoTimeout
    : public ActorToolsTest,
      public testing::WithParamInterface<bool> {
 public:
  ActorToolsTestScriptToolNoTimeout() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kWebMCP, {}},
        {blink::features::kDevToolsWebMCPSupport, {}},
        {actor::kGlicActorEnableScriptTools, {{"execution_timeout", "1s"}}}};

    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam()) {
      enabled_features.push_back({::features::kBackForwardCache, {}});
    } else {
      disabled_features.push_back(::features::kBackForwardCache);
    }

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptToolNoTimeout,
                       DeclarativeToolNoTimeout) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_pause.html");
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
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorToolsTestScriptToolNoTimeout,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "BFCacheEnabled"
                                             : "BFCacheDisabled";
                         });
IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, Histograms) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, NavigationFailed) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document.html");
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

  const std::string declarative_input = R"JSON({"echo": "hello world"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kScriptToolNavigationDidNotCommit);
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, NavigationCommittedErrorPage) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Change form action to a non-existent path on the same server, which should
  // result in an error page (404).
  const GURL error_url = embedded_test_server()->GetURL("/non-existent");
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("document.querySelector('form').action = $1",
                         error_url)));

  const std::string declarative_input = R"JSON({"echo": "hello world"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  ExpectErrorResult(
      result, mojom::ActionResultCode::kScriptToolNavigationCommittedErrorPage);
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, NavigationFailedLoad) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL fail_url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document_fail.html");
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("document.querySelector('form').action = $1",
                         fail_url)));

  const std::string declarative_input = R"JSON({"echo": "hello world"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kScriptToolNavigationFailedLoad);
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, NavigationBlockedByCSP) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Inject a CSP meta tag to block form submission.
  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    var meta = document.createElement('meta');
    meta.httpEquiv = 'Content-Security-Policy';
    meta.content = "form-action 'none'";
    document.getElementsByTagName('head')[0].appendChild(meta);
  )"));

  const std::string declarative_input = R"JSON({"echo": "hello world"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kScriptToolNavigationDidNotCommit);
}
IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool,
                       OtherFrameNavigationDoesNotCancelTool) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_slow.html");
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool,
                       SameDocumentNavigationDoesNotCancelTool) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_slow.html");
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool,
                       UnrelatedNavigationCancelsTool) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_slow.html");
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
      web_contents(), embedded_test_server()->GetURL("/title1.html")));
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  // The tool should be cancelled because it was replaced by an unrelated
  // navigation.
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolCancelled);
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool,
                       TabClosedWhileWaitingForNavigationDoesNotCrash) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_slow.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    navigator.modelContext.registerTool({
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
      web_contents(), embedded_test_server()->GetURL("/title1.html"));
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ASSERT_TRUE(nav_manager.WaitForRequestStart());

  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  auto act_result = result.Get();
  EXPECT_FALSE(act_result.empty());
}
IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool,
                       BrowserInitiatedBackNavigationFailsTool) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_slow.html");
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

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, ToolSelfNavigates) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_self_navigate.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action =
      MakeScriptToolRequest(*main_frame(), "navigate", input_arguments);
  content::TestNavigationObserver nav_observer(web_contents());
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  auto response = result.Get();
  ExpectOkResult(*response[0].result);
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, ToolNavigatesAsyncTask) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/script_tool_self_navigate_delayed.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action =
      MakeScriptToolRequest(*main_frame(), "navigate_delayed", input_arguments);
  content::TestNavigationObserver nav_observer(web_contents());
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());

  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  auto response = result.Get();
  ExpectOkResult(*response[0].result);
}

IN_PROC_BROWSER_TEST_P(ActorToolsTestScriptTool, ToolReentrantExecution) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_slow.html");
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

IN_PROC_BROWSER_TEST_P(
    ActorToolsTestScriptTool,
    BrowserInitiatedBackNavigationWhileWaitingForUserFailsTool) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title1.html")));
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_pause.html");
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

INSTANTIATE_TEST_SUITE_P(All,
                         ActorToolsTestScriptTool,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "BFCacheEnabled"
                                             : "BFCacheDisabled";
                         });

}  // namespace actor
