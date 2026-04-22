// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
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
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebMCP, actor::kGlicActorEnableScriptTools}, {});
  }
  ~GlicActorWithScriptToolsTest() override = default;

  auto ScriptAction(const std::string& name,
                    const std::string& input_arguments,
                    std::optional<actor::mojom::ActionResultCode>
                        expected_result_code = std::nullopt) {
    auto script_provider = base::BindLambdaForTesting([&]() {
      Actions action = actor::MakeScriptTool(
          *tab_handle_.Get()->GetContents()->GetPrimaryMainFrame(), name,
          input_arguments, task_id_);
      return EncodeActionProto(action);
    });
    return expected_result_code ? ExecuteAction(std::move(script_provider),
                                                *expected_result_code)
                                : ExecuteAction(std::move(script_provider), {});
  }

  auto CheckValidResult(const std::string& name,
                        const std::string& input_arguments,
                        const std::string& expected_result) {
    return Steps(Do([=, this]() {
      ASSERT_TRUE(last_execution_result());
      ASSERT_EQ(last_execution_result()->script_tool_results().size(), 1);
      EXPECT_EQ(last_execution_result()->action_result(),
                static_cast<int32_t>(actor::mojom::ActionResultCode::kOk));
      EXPECT_FALSE(last_execution_result()->has_index_of_failed_action());
      EXPECT_FALSE(last_execution_result()->has_error_message());

      const auto& result = last_execution_result()->script_tool_results().at(0);
      EXPECT_EQ(result.index_of_script_tool_action(), 0);
      EXPECT_EQ(result.result(), expected_result);
      EXPECT_EQ(result.tool().name(), name);
      EXPECT_EQ(result.input_arguments(), input_arguments);

      ASSERT_EQ(last_execution_result()->tabs().size(), 1);
      const auto& apc =
          last_execution_result()->tabs().at(0).annotated_page_content();
      const auto& main_frame_data = apc.main_frame_data();
      ASSERT_EQ(main_frame_data.script_tool_results().size(), 1);

      const auto& apc_result = main_frame_data.script_tool_results().at(0);
      EXPECT_EQ(apc_result.result(), expected_result);
      EXPECT_EQ(apc_result.tool_name(), name);
      EXPECT_EQ(apc_result.input_arguments(), input_arguments);
    }));
  }

  auto CheckErrorResult(const std::string& name,
                        const std::string& input_arguments,
                        const actor::mojom::ActionResultCode expected_code,
                        const std::string& expected_message,
                        bool has_tab_observation) {
    return Steps(Do([=, this]() {
      ASSERT_TRUE(last_execution_result());
      EXPECT_EQ(last_execution_result()->action_result(),
                static_cast<int32_t>(expected_code));
      EXPECT_EQ(last_execution_result()->index_of_failed_action(), 0);
      EXPECT_EQ(last_execution_result()->error_message(), expected_message);

      ASSERT_EQ(last_execution_result()->script_tool_results().size(), 0);

      if (has_tab_observation) {
        EXPECT_EQ(last_execution_result()->tabs().size(), 1);
        const auto& apc =
            last_execution_result()->tabs().at(0).annotated_page_content();
        const auto& main_frame_data = apc.main_frame_data();
        ASSERT_EQ(main_frame_data.script_tool_results().size(), 0);
      } else {
        EXPECT_EQ(last_execution_result()->tabs().size(), 0);
      }
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
                  CheckValidResult("echo", input_arguments, expected_result));
}

IN_PROC_BROWSER_TEST_F(GlicActorWithScriptToolsTest, InvalidToolName) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/script_tool.html");

  const std::string input_arguments = "{}";
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      ScriptAction("invalid", input_arguments,
                   actor::mojom::ActionResultCode::kScriptToolInvalidName),
      CheckErrorResult("invalid", input_arguments,
                       actor::mojom::ActionResultCode::kScriptToolInvalidName,
                       "Tool not found: invalid",
                       true /* has_tab_observation */));
}

class GlicActorWithScriptToolsDisabledTest
    : public GlicActorWithScriptToolsTest {
 public:
  GlicActorWithScriptToolsDisabledTest() {
    scoped_feature_list_.InitWithFeatures({blink::features::kWebMCP},
                                          {actor::kGlicActorEnableScriptTools});
  }
  ~GlicActorWithScriptToolsDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorWithScriptToolsDisabledTest, Disabled) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/script_tool.html");

  const std::string input_arguments = "{}";
  // If the feature is disabled, CreateToolRequest returns nullptr for
  // ScriptToolAction, which results in kArgumentsInvalid from
  // GlicActorTaskManager::PerformActions.
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      ScriptAction("echo", input_arguments,
                   actor::mojom::ActionResultCode::kArgumentsInvalid),
      CheckErrorResult("echo", input_arguments,
                       actor::mojom::ActionResultCode::kArgumentsInvalid, "",
                       false /* has_tab_observation */));
}

}  //  namespace

}  // namespace glic::test
