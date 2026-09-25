// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/tool_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/tool_types.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class ToolControllerBrowserTest : public TtcCoreBrowserTestBase {
 public:
  ToolControllerBrowserTest() = default;
  ~ToolControllerBrowserTest() override = default;

  // TtcCoreBrowserTestBase:
  void SetUpOnMainThread() override {
    TtcCoreBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenUrlCurrentTab) {
  // Verify ActorKeyedService is available.
  auto* actor_service = actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ToolRequest tool_request;
  tool_request.name = "open_url";
  tool_request.arguments.Set("url", url.spec());
  tool_request.arguments.Set("new_tab", false);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, UnsupportedTool) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name = "unsupported_tool";

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kToolUnknown);
  ASSERT_TRUE(response.error().message.has_value());
  EXPECT_EQ(*response.error().message, "Unsupported tool");
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GetToolDefinitions) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  std::vector<ToolDefinition> tools = session_controller->GetToolDefinitions();
  ASSERT_EQ(tools.size(), 1u);

  const ToolDefinition& open_url = tools[0];
  EXPECT_EQ(open_url.name, "open_url");
  EXPECT_FALSE(open_url.description.empty());
  EXPECT_EQ(open_url.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(open_url.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& schema = open_url.parameters_json_schema;
  const std::string* schema_type = schema.FindString("type");
  ASSERT_TRUE(schema_type);
  EXPECT_EQ(*schema_type, "object");

  const std::string* url_type =
      schema.FindStringByDottedPath("properties.url.type");
  ASSERT_TRUE(url_type);
  EXPECT_EQ(*url_type, "string");

  const std::string* new_tab_type =
      schema.FindStringByDottedPath("properties.new_tab.type");
  ASSERT_TRUE(new_tab_type);
  EXPECT_EQ(*new_tab_type, "boolean");

  const base::ListValue* required = schema.FindList("required");
  ASSERT_TRUE(required);
  EXPECT_EQ(*required, base::ListValue().Append("url").Append("new_tab"));
}

// TTC actor tasks are given TtcKeyedService's ActorUiStateManager rather than
// the profile-wide one, so none of the tab-scoped actor UI the latter drives
// (the actor overlay, the handoff button, the tab indicator, the border glow)
// should be shown while TTC acts on a tab.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       OpenUrlDoesNotShowTabScopedActorUi) {
  auto* actor_service = actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents());
  ASSERT_TRUE(tab);
  actor::ui::ActorUiTabControllerInterface* tab_controller =
      actor::ui::ActorUiTabControllerInterface::From(tab);
  ASSERT_TRUE(tab_controller);
  ASSERT_EQ(tab_controller->GetCurrentUiTabState(), actor::ui::UiTabState());

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ToolRequest tool_request;
  tool_request.name = "open_url";
  tool_request.arguments.Set("url", url.spec());
  tool_request.arguments.Set("new_tab", false);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());
  ASSERT_TRUE(future.Take().Ok());
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), url);

  // No UI should be showing as the TTC specific ActorUiStateManagerInterface
  // is used.
  EXPECT_EQ(tab_controller->GetCurrentUiTabState(), actor::ui::UiTabState());
  ttc_service().EndSession();

  // Control: the same navigation, performed by a task that is given the
  // profile-wide state manager, does show UI.

  const actor::TaskId control_task_id = actor_service->CreateTaskWithOptions(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest, "control"),
      actor::GetNullEnterprisePolicyChecker(), /*options=*/nullptr,
      /*delegate=*/nullptr, actor::ui::ActorUiStateManager::Get(profile()));

  std::vector<std::unique_ptr<actor::ToolRequest>> actions;
  actions.push_back(std::make_unique<actor::NavigateToolRequest>(
      tab->GetHandle(),
      embedded_https_test_server().GetURL("example.com", "/title2.html")));
  base::test::TestFuture<std::vector<actor::ActionResultWithLatencyInfo>,
                         actor::TabObservationStrategy>
      actions_result;
  actor_service->PerformActions(control_task_id, std::move(actions),
                                actor::ActorTaskMetadata(),
                                actions_result.GetCallback());
  ASSERT_TRUE(actions_result.Wait());
  ASSERT_TRUE(actor::IsOk(*actions_result.Get<0>()[0].result));

  // There should be actor UI showing now.
  EXPECT_NE(tab_controller->GetCurrentUiTabState(), actor::ui::UiTabState());

  actor_service->StopTask(control_task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
}

}  // namespace

}  // namespace ttc
