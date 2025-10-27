// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
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
  EXPECT_EQ(*action_results.at(0).result->script_tool_response,
            "This is an example sentence.");
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
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
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
  EXPECT_EQ(*echo_action_results.at(0).result->script_tool_response,
            "Hello World");

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
  EXPECT_EQ(*reverse_action_results.at(0).result->script_tool_response,
            "321cba");
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
  ExpectErrorResult(echo_result_after_clear, mojom::ActionResultCode::kError);
}

}  // namespace
}  // namespace actor
