// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features_generated.h"

using base::test::TestFuture;

namespace actor {

namespace {

class ActorToolsTestScriptTool : public ActorToolsTest {
 public:
  ActorToolsTestScriptTool() {
    features_.InitAndEnableFeature(blink::features::kWebMCP);
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, Basic) {
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string input_arguments =
      R"JSON(
        { "text": "This is an example sentence." }
      )JSON";
  auto action = MakeScriptToolRequest(*main_frame(), "echo", input_arguments);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& action_results = result.Get<2>();
  ASSERT_EQ(action_results.size(), 1u);
  ASSERT_TRUE(action_results.at(0).result->script_tool_response);
  EXPECT_EQ(action_results.at(0).result->script_tool_response->result,
            "This is an example sentence.");
  EXPECT_EQ(action_results.at(0).result->script_tool_response->name, "echo");
  EXPECT_EQ(action_results.at(0).result->script_tool_response->input_arguments,
            input_arguments);
  EXPECT_EQ(action_results.at(0).result->script_tool_response->tool->name,
            "echo");
  EXPECT_EQ(
      action_results.at(0).result->script_tool_response->tool->description,
      "echo input");
  EXPECT_EQ(action_results.at(0)
                .result->script_tool_response->tool->annotations->read_only,
            true);

  const std::string expected_input_schema =
      R"JSON({"type":"object","properties":{"text":{"description":)JSON"
      R"JSON("Value to echo","type":"string"}},"required":["text"]})JSON";
  EXPECT_EQ(
      action_results.at(0).result->script_tool_response->tool->input_schema,
      expected_input_schema);
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

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, ProvideContext) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_provide_context.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string echo_input =
      R"JSON(
        { "text": "Hello World" }
      )JSON";
  auto echo_action = MakeScriptToolRequest(*main_frame(), "echo", echo_input);
  ActResultFuture echo_result;
  actor_task().Act(ToRequestList(echo_action), echo_result.GetCallback());
  ExpectOkResult(echo_result);

  const auto& echo_action_results = echo_result.Get<2>();
  ASSERT_EQ(echo_action_results.size(), 1u);
  ASSERT_TRUE(echo_action_results.at(0).result->script_tool_response);
  EXPECT_EQ(echo_action_results.at(0).result->script_tool_response->result,
            "Hello World");
  EXPECT_EQ(echo_action_results.at(0).result->script_tool_response->name,
            "echo");
  EXPECT_EQ(
      echo_action_results.at(0).result->script_tool_response->input_arguments,
      echo_input);

  const std::string reverse_input =
      R"JSON(
        { "text": "abc123" }
      )JSON";
  auto reverse_action =
      MakeScriptToolRequest(*main_frame(), "reverse", reverse_input);
  ActResultFuture reverse_result;
  actor_task().Act(ToRequestList(reverse_action), reverse_result.GetCallback());
  ExpectOkResult(reverse_result);

  const auto& reverse_action_results = reverse_result.Get<2>();
  ASSERT_EQ(reverse_action_results.size(), 1u);
  ASSERT_TRUE(reverse_action_results.at(0).result->script_tool_response);
  EXPECT_EQ(reverse_action_results.at(0).result->script_tool_response->result,
            "321cba");
  EXPECT_EQ(reverse_action_results.at(0).result->script_tool_response->name,
            "reverse");
  EXPECT_EQ(reverse_action_results.at(0)
                .result->script_tool_response->input_arguments,
            reverse_input);
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, ClearContext) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/script_tool_provide_context.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string echo_input =
      R"JSON(
        { "text": "test" }
      )JSON";
  auto echo_action = MakeScriptToolRequest(*main_frame(), "echo", echo_input);
  ActResultFuture echo_result;
  actor_task().Act(ToRequestList(echo_action), echo_result.GetCallback());
  ExpectOkResult(echo_result);

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "navigator.modelContext.clearContext();"));

  auto echo_action_after_clear =
      MakeScriptToolRequest(*main_frame(), "echo", echo_input);
  ActResultFuture echo_result_after_clear;
  actor_task().Act(ToRequestList(echo_action_after_clear),
                   echo_result_after_clear.GetCallback());
  ExpectErrorResult(echo_result_after_clear,
                    mojom::ActionResultCode::kScriptToolInvalidName);
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& action_results = result.Get<2>();
  ASSERT_EQ(action_results.size(), 1u);
  ASSERT_TRUE(action_results.at(0).result->script_tool_response);
  EXPECT_EQ(action_results.at(0).result->script_tool_response->name,
            "declarative_tool");
  EXPECT_EQ(action_results.at(0).result->script_tool_response->input_arguments,
            declarative_input);
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectOkResult(result);

  const auto& action_results = result.Get<2>();
  ASSERT_EQ(action_results.size(), 1u);
  ASSERT_TRUE(action_results.at(0).result->script_tool_response);
  EXPECT_EQ(action_results.at(0).result->script_tool_response->result,
            "This is an example sentence.");
}

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, DeclarativeToolCrossDocument) {
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& action_results = result.Get<2>();
  ASSERT_EQ(action_results.size(), 1u);
  ASSERT_TRUE(action_results.at(0).result->script_tool_response);
  EXPECT_EQ(action_results.at(0).result->script_tool_response->name,
            "declarative_tool");
  EXPECT_EQ(action_results.at(0).result->script_tool_response->input_arguments,
            declarative_input);
  EXPECT_EQ(action_results.at(0).result->script_tool_response->tool->name,
            "declarative_tool");
  EXPECT_EQ(
      action_results.at(0).result->script_tool_response->tool->description,
      "A declarative WebMCP tool");
  EXPECT_FALSE(
      action_results.at(0).result->script_tool_response->tool->annotations);
  EXPECT_EQ(
      action_results.at(0).result->script_tool_response->tool->input_schema,
      "{}");

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

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool,
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

  const auto& action_results = result.Get<2>();
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

}  // namespace
}  // namespace actor
