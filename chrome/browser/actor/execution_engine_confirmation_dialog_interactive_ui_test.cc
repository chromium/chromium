// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace actor {

namespace {

constexpr char kHandleDialogRequestTempl[] =
    R"js(
  (() => {
    window.userConfirmationDialogRequestData = new Promise(resolve => {
      client.browser.selectUserConfirmationDialogRequestHandler().subscribe(
        request => {
          // Response will be verified in C++ callback below.
          request.onDialogClosed({
            response: {
              taskId: request.taskId,
              permissionGranted: $1,
            }
          });
          // Resolve the promise with the request data to be verified.
          resolve({
            taskId: request.taskId,
            navigationOrigin: request.navigationOrigin,
            downloadId: request.downloadId,
          });
        }
      );
    });
  })();
)js";

}  // namespace

class ExecutionEngineConfirmationDialogInteractiveUiTest
    : public glic::test::InteractiveGlicTest {
 public:
  ExecutionEngineConfirmationDialogInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor,
                              kGlicCrossOriginNavigationGating},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ~ExecutionEngineConfirmationDialogInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    auto execution_engine =
        std::make_unique<ExecutionEngine>(browser()->profile());
    auto event_dispatcher = ui::NewUiEventDispatcher(
        ActorKeyedService::Get(browser()->profile())->GetActorUiStateManager());
    auto actor_task = std::make_unique<ActorTask>(browser()->profile(),
                                                  std::move(execution_engine),
                                                  std::move(event_dispatcher));
    task_id_ = ActorKeyedService::Get(browser()->profile())
                   ->AddActiveTask(std::move(actor_task));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
  TaskId task_id() { return task_id_; }

  ActorTask* GetActorTask() {
    return ActorKeyedService::Get(browser()->profile())->GetTask(task_id_);
  }

  InteractiveTestApi::MultiStep CreateMockUserConfirmationDialog(
      const std::string_view handle_dialog_js) {
    return InAnyContext(WithElement(
        glic::test::kGlicContentsElementId,
        [handle_dialog_js](::ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          ASSERT_TRUE(content::ExecJs(glic_contents, handle_dialog_js));
        }));
  }

  InteractiveTestApi::MultiStep VerifyUserConfirmationDialogRequest(
      const base::Value::Dict& expected_request) {
    return InAnyContext(WithElement(
        glic::test::kGlicContentsElementId, [&](::ui::TrackedElement* el) {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          static constexpr char kGetRequestData[] =
              R"js(
              (() => {
                return window.userConfirmationDialogRequestData;
              })();
            )js";
          auto eval_result = content::EvalJs(glic_contents, kGetRequestData);
          const auto& actual_request = eval_result.ExtractDict();
          ASSERT_EQ(expected_request, actual_request);
        }));
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }
  ActorTask& actor_task() {
    return *ActorKeyedService::Get(browser()->profile())->GetTask(task_id_);
  }

  void ClickTarget(
      std::string_view query_selector,
      mojom::ActionResultCode expected_code = mojom::ActionResultCode::kOk) {
    std::optional<int> dom_node_id =
        content::GetDOMNodeId(*main_frame(), query_selector);
    ASSERT_TRUE(dom_node_id);
    std::unique_ptr<ToolRequest> click =
        MakeClickRequest(*main_frame(), dom_node_id.value());
    ActResultFuture result;
    actor_task().Act(ToRequestList(click), result.GetCallback());
    if (expected_code == mojom::ActionResultCode::kOk) {
      ExpectOkResult(result);
    } else {
      ExpectErrorResult(result, expected_code);
    }
  }

 private:
  TaskId task_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExecutionEngineConfirmationDialogInteractiveUiTest,
                       PromptToConfirmCrossOriginNavigation) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CreateMockUserConfirmationDialog(
                      content::JsReplace(kHandleDialogRequestTempl, true)));

  base::test::TestFuture<webui::mojom::UserConfirmationDialogResponsePtr>
      future;
  GetActorTask()->GetExecutionEngine()->PromptToConfirmCrossOriginNavigation(
      url::Origin::Create(GURL("https://www.example.com")),
      future.GetCallback());

  // Verify response was forwarded to the callback correctly.
  auto response = future.Take();
  EXPECT_TRUE(response->result->is_permission_granted());
  EXPECT_TRUE(response->result->get_permission_granted());
  EXPECT_FALSE(response->result->is_error_reason());

  auto expected_request = base::Value::Dict().Set(
      "navigationOrigin", "https://www.example.com:443");
  RunTestSequence(VerifyUserConfirmationDialogRequest(expected_request));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineConfirmationDialogInteractiveUiTest,
                       CrossOriginNavigationGating_Granted) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CreateMockUserConfirmationDialog(
                      content::JsReplace(kHandleDialogRequestTempl, true)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link", mojom::ActionResultCode::kOk);
  auto expected_request = base::Value::Dict().Set(
      "navigationOrigin", url::Origin::Create(second_url).GetDebugString());
  RunTestSequence(VerifyUserConfirmationDialogRequest(expected_request));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineConfirmationDialogInteractiveUiTest,
                       CrossOriginNavigationGating_Denied) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CreateMockUserConfirmationDialog(
                      content::JsReplace(kHandleDialogRequestTempl, false)));

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", start_url)));
  ClickTarget("#link", mojom::ActionResultCode::kOk);

  EXPECT_TRUE(content::ExecJs(web_contents(),
                              content::JsReplace("setLink($1);", second_url)));

  ClickTarget("#link", mojom::ActionResultCode::kTriggeredNavigationBlocked);
  auto expected_request = base::Value::Dict().Set(
      "navigationOrigin", url::Origin::Create(second_url).GetDebugString());
  RunTestSequence(VerifyUserConfirmationDialogRequest(expected_request));
}

IN_PROC_BROWSER_TEST_F(ExecutionEngineConfirmationDialogInteractiveUiTest,
                       PromptToConfirmDownload) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CreateMockUserConfirmationDialog(
                      content::JsReplace(kHandleDialogRequestTempl, true)));

  base::test::TestFuture<webui::mojom::UserConfirmationDialogResponsePtr>
      future;
  GetActorTask()->GetExecutionEngine()->PromptToConfirmDownload(
      /*download_id=*/123, future.GetCallback());

  // Verify response was forwarded to the callback correctly.
  auto response = future.Take();
  EXPECT_TRUE(response->result->is_permission_granted());
  EXPECT_TRUE(response->result->get_permission_granted());
  EXPECT_FALSE(response->result->is_error_reason());

  auto expected_request = base::Value::Dict().Set("downloadId", 123);
  RunTestSequence(VerifyUserConfirmationDialogRequest(expected_request));
}

}  // namespace actor
