// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features_generated.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;

class GlicActorWithScriptToolsTest : public GlicActorUiTest {
 public:
  GlicActorWithScriptToolsTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kWebMCP);
  }
  ~GlicActorWithScriptToolsTest() override = default;

  auto ScriptAction(const std::string& name,
                    const std::string& input_arguments) {
    auto script_provider = base::BindLambdaForTesting([&]() {
      Actions action = actor::MakeScriptTool(
          *tab_handle_.Get()->GetContents()->GetPrimaryMainFrame(), name,
          input_arguments);
      action.set_task_id(task_id_.value());
      return EncodeActionProto(action);
    });
    return ExecuteAction(std::move(script_provider), {});
  }

  auto CheckValidResult(const std::string& expected_result) {
    return Steps(Do([&]() {
      ASSERT_TRUE(last_execution_result());
      ASSERT_EQ(last_execution_result()->script_tool_results().size(), 1);

      const auto& result = last_execution_result()->script_tool_results().at(0);
      EXPECT_EQ(result.index_of_script_tool_action(), 0);
      EXPECT_EQ(result.result(), expected_result);
    }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorWithScriptToolsTest, Basic) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/script_tool.html");

  const std::string expected_result = "This is an example sentence.";
  const std::string input_arguments =
      "{ \"text\": \"This is an example sentence.\" }";
  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  ScriptAction("echo", input_arguments),
                  CheckValidResult(expected_result));
}

}  //  namespace

}  // namespace glic::test
