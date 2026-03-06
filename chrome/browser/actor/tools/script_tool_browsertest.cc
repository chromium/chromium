// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  actor::mojom::ScriptToolResponsePtr RunScriptTool(
      std::unique_ptr<ToolRequest> action) {
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);

    const auto& action_results = result.Get<2>();
    EXPECT_EQ(action_results.size(), 1u);
    EXPECT_TRUE(action_results.at(0).result);
    actor::mojom::ScriptToolResponsePtr response =
        std::move(action_results.at(0).result->script_tool_response);
    EXPECT_TRUE(response);
    return response;
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
  auto response = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "This is an example sentence.");
  EXPECT_EQ(response->input_arguments, input_arguments);
  EXPECT_EQ(response->tool->name, "echo");
  EXPECT_EQ(response->tool->description, "echo input");
  EXPECT_EQ(response->tool->annotations->read_only, true);

  const std::string expected_input_schema =
      R"JSON({"type":"object","properties":{"text":{"description":)JSON"
      R"JSON("Value to echo","type":"string"}},"required":["text"]})JSON";
  EXPECT_EQ(response->tool->input_schema, expected_input_schema);
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
  auto response = RunScriptTool(std::move(action));
  EXPECT_EQ(response->tool->name, "declarative_tool");
  EXPECT_EQ(response->input_arguments, declarative_input);
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
  auto response = RunScriptTool(std::move(action));
  EXPECT_EQ(response->result, "This is an example sentence.");
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

  auto response = RunScriptTool(std::move(action));
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

IN_PROC_BROWSER_TEST_F(ActorToolsTestScriptTool, Histograms) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL("/actor/script_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string valid_input_arguments = R"JSON({"text": "test"})JSON";
  auto action =
      MakeScriptToolRequest(*main_frame(), "echo", valid_input_arguments);
  auto response = RunScriptTool(std::move(action));

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

}  // namespace
}  // namespace actor
