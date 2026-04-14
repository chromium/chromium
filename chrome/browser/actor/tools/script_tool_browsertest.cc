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
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

using base::test::TestFuture;

namespace actor {

namespace {

class TestDevToolsClient : public ::content::DevToolsAgentHostClient {
 public:
  TestDevToolsClient() = default;
  ~TestDevToolsClient() override { Detach(); }

  void AttachToAndEnableWebMCP(
      scoped_refptr<content::DevToolsAgentHost> agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);

    const std::string enable_message =
        R"JSON({"id": 1, "method": "WebMCP.enable"})JSON";
    agent_host_->DispatchProtocolMessage(this,
                                         base::as_byte_span(enable_message));
  }

  void Detach() {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    std::string_view message_str(reinterpret_cast<const char*>(message.data()),
                                 message.size());
    std::optional<base::Value> parsed = base::test::ParseJson(message_str);
    if (!parsed || !parsed->is_dict()) {
      return;
    }

    const base::DictValue& dict = parsed->GetDict();

    const std::string* method = dict.FindString("method");
    if (!method) {
      return;
    }

    if (*method == "WebMCP.toolInvoked") {
      invoked_events_.push_back(std::move(*parsed));
    } else if (*method == "WebMCP.toolResponded") {
      responded_events_.push_back(std::move(*parsed));
    }
  }

  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}

  const std::vector<base::Value>& invoked_events() const {
    return invoked_events_;
  }
  const std::vector<base::Value>& responded_events() const {
    return responded_events_;
  }

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::vector<base::Value> invoked_events_;
  std::vector<base::Value> responded_events_;
};
class ActorToolsTestScriptTool : public ActorToolsTest {
 public:
  ActorToolsTestScriptTool() {
    features_.InitWithFeatures(
        {blink::features::kWebMCP, blink::features::kDevToolsWebMCPSupport,
         actor::kGlicActorEnableScriptTools},
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
    actor_task().Act(ToRequestList(action), result.GetCallback());
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

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptToolWithStability,
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

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, Basic) {
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

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, EmitsCdpEvents) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  auto [action_result, response] = RunScriptTool(std::move(action));

  EXPECT_EQ(response->result, "test_input");

  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "echo");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  EXPECT_EQ(*input, input_arguments);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  ASSERT_EQ(client.responded_events().size(), 1u);
  const base::DictValue& responded_event =
      client.responded_events()[0].GetDict();
  const base::DictValue* responded_params = responded_event.FindDict("params");
  ASSERT_TRUE(responded_params);
  const std::string* responded_invocation_id =
      responded_params->FindString("invocationId");
  ASSERT_TRUE(responded_invocation_id);
  EXPECT_EQ(*responded_invocation_id, *invocation_id);
  const std::string* status = responded_params->FindString("status");
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, "Completed");

  client.Detach();
}
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, BadToolName) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments =
      R"JSON(
        { "text": "This is an example sentence." }
      )JSON";
  auto action =
      MakeScriptToolRequest(*main_frame(), "invalid", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolInvalidName);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, EmitsCdpEventsOnFailure) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

  const std::string input_arguments = R"JSON({"text": "test_input"})JSON";
  auto action =
      MakeScriptToolRequest(*main_frame(), "invalid", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolInvalidName);

  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "invalid");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  EXPECT_EQ(*input, input_arguments);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  ASSERT_EQ(client.responded_events().size(), 1u);
  const base::DictValue& responded_event =
      client.responded_events()[0].GetDict();
  const base::DictValue* responded_params = responded_event.FindDict("params");
  ASSERT_TRUE(responded_params);
  const std::string* responded_invocation_id =
      responded_params->FindString("invocationId");
  ASSERT_TRUE(responded_invocation_id);
  EXPECT_EQ(*responded_invocation_id, *invocation_id);
  const std::string* status = responded_params->FindString("status");
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, "Error");
  const std::string* error_text = responded_params->FindString("errorText");
  ASSERT_TRUE(error_text);
  EXPECT_EQ(*error_text, "Tool not found: invalid");

  client.Detach();
}
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, DeclarativeTool) {
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
  EXPECT_EQ(response->input_arguments, declarative_input);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       EmitsCdpEventsDeclarativeTool) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/declarative_script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

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

  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "declarative_tool");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  // Compare parsed JSON to ignore whitespace differences
  std::optional<base::Value> parsed_input = base::test::ParseJson(*input);
  std::optional<base::Value> parsed_expected_input =
      base::test::ParseJson(declarative_input);
  ASSERT_TRUE(parsed_input);
  ASSERT_TRUE(parsed_expected_input);
  EXPECT_EQ(*parsed_input, *parsed_expected_input);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  ASSERT_EQ(client.responded_events().size(), 1u);
  const base::DictValue& responded_event =
      client.responded_events()[0].GetDict();
  const base::DictValue* responded_params = responded_event.FindDict("params");
  ASSERT_TRUE(responded_params);
  const std::string* responded_invocation_id =
      responded_params->FindString("invocationId");
  ASSERT_TRUE(responded_invocation_id);
  EXPECT_EQ(*responded_invocation_id, *invocation_id);
  const std::string* status = responded_params->FindString("status");
  ASSERT_TRUE(status);
  EXPECT_EQ(*status, "Completed");

  client.Detach();
}
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigateAfterResponse) {
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

// TODO(crbug.com/492477322): Re-enable this test.
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, DISABLED_DeclarativeToolCrossDocument) {
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
  EXPECT_EQ(response->input_arguments, declarative_input);
  EXPECT_EQ(response->tool->name, "declarative_tool");
  EXPECT_EQ(response->tool->description, "A declarative WebMCP tool");
  EXPECT_FALSE(response->tool->annotations);
  EXPECT_EQ(response->tool->input_schema, "{}");

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

// TODO(crbug.com/496357393): Re-enable this test.
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       DISABLED_EmitsCdpEventsDeclarativeToolCrossDocument) {
  const GURL url = embedded_test_server()->GetURL(
      "/actor/declarative_script_tool_cross_document.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  TestDevToolsClient client;
  client.AttachToAndEnableWebMCP(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents()));

  const std::string declarative_input =
      R"JSON(
        {
          "echo": "hello world"
        }
      )JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "declarative_tool",
                                      declarative_input);

  auto [action_result, response] = RunScriptTool(std::move(action));
  EXPECT_EQ(response->input_arguments, declarative_input);
  EXPECT_EQ(response->tool->name, "declarative_tool");

  ASSERT_EQ(client.invoked_events().size(), 1u);
  const base::DictValue& invoked_event = client.invoked_events()[0].GetDict();
  const base::DictValue* invoked_params = invoked_event.FindDict("params");
  ASSERT_TRUE(invoked_params);
  const std::string* tool_name = invoked_params->FindString("toolName");
  ASSERT_TRUE(tool_name);
  EXPECT_EQ(*tool_name, "declarative_tool");
  const std::string* input = invoked_params->FindString("input");
  ASSERT_TRUE(input);
  EXPECT_EQ(*input, declarative_input);
  const std::string* invocation_id = invoked_params->FindString("invocationId");
  ASSERT_TRUE(invocation_id);

  // Cross document tools do not send a WebMCP.toolResponded event.
  EXPECT_EQ(client.responded_events().size(), 0u);

  client.Detach();
}

// TODO(crbug.com/496357393): Re-enable this test.
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
                       DISABLED_DeclarativeToolCrossDocument_No_Autosubmit) {
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
  actor_task().Act(ToRequestList(action), result.GetCallback());

  // Wait for the task to be paused. The Act() call has not returned yet.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return actor_task().GetState() == ActorTask::State::kPausedByActor;
  }));

  // Trigger the submission manually.
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.querySelector('button').click()"));

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

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, Histograms) {
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
  actor_task().Act(ToRequestList(bad_action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kScriptToolInvalidName);

  histogram_tester.ExpectBucketCount("Actor.Tools.ScriptTool.InputSizeBytes",
                                     input_arguments.size(), 1);
  histogram_tester.ExpectBucketCount("Actor.Tools.ScriptTool.InvocationResult",
                                     false, 1);
  histogram_tester.ExpectBucketCount(
      "Actor.Tools.ScriptTool.ActionResultCode",
      mojom::ActionResultCode::kScriptToolInvalidName, 1);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigationFailed) {
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
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kScriptToolNavigationDidNotCommit);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigationCommittedErrorPage) {
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
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(
      result, mojom::ActionResultCode::kScriptToolNavigationCommittedErrorPage);
}

// TODO(crbug.com/492477322): Enable for bfcache.
IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, NavigationFailedLoad) {
  if (!base::FeatureList::IsEnabled(features::kBackForwardCache)) {
    GTEST_SKIP() << "Skipping when bfcache is disabled; crbug.com/492477322";
  }

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
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kScriptToolNavigationFailedLoad);
}

}  // namespace
}  // namespace actor
