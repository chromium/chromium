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
    features_.InitAndEnableFeature(blink::features::kScriptTools);
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

  const auto& script_tool_results = result.Get<2>();
  ASSERT_EQ(script_tool_results.size(), 1u);
  EXPECT_EQ(script_tool_results.at(0).index_of_script_tool_action(), 0);
  EXPECT_EQ(script_tool_results.at(0).result(), "This is an example sentence.");
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

}  // namespace
}  // namespace actor
