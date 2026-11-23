// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/javascript_dialog_interceptor.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/execute_js_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace actor {
namespace {

class JavaScriptDialogInterceptorBrowserTest : public ActorToolsTest {
 public:
  JavaScriptDialogInterceptorBrowserTest() {
    features_.InitWithFeatures(
        {features::kGlicActor, kActorInterceptJavaScriptDialogs},
        {kGlicSkipBeforeUnloadDialogAndNavigate});
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  GURL GetTestPageUrl(std::string_view path = "/title1.html") {
    return embedded_https_test_server().GetURL("example.com", path);
  }

  std::unique_ptr<ToolRequest> MakeExecuteJsRequest(
      ActorSurfaceHandle surface,
      content::RenderFrameHost* frame,
      const std::string& script_code,
      uint32_t timeout_ms = 5000) {
    auto document_id = optimization_guide::DocumentIdentifierUserData::
                           GetOrCreateForCurrentDocument(frame)
                               ->token();
    return std::make_unique<ExecuteJsToolRequest>(surface, document_id,
                                                  script_code, timeout_ms);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Clicking a button whose main-world handler triggers an async callback
// calling window.alert() must not hang the renderer or show modal UI, and must
// return the intercepted dialog in the action receipt.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       InterceptsMainWorldAsyncAlertFromClick) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      GetTestPageUrl("/actor/page_with_clickable_element.html")));

  ASSERT_TRUE(content::ExecJs(web_contents(), R"JS(
    window.waitlist_done = false;
    document.getElementById('clickable').onclick = () => {
      Promise.resolve().then(() => {
        window.alert('Successfully added to waitlist');
        window.waitlist_done = true;
      });
    };
  )JS"));

  std::optional<int> button_id =
      content::GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeClickRequest(*main_frame(), button_id.value())),
      result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(true, content::EvalJs(web_contents(), "window.waitlist_done"));

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "Successfully added to waitlist");
}

// Intercepts alerts and confirms fired in a backgrounded actor tab without
// queueing a stale modal dialog when the tab is later re-activated.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       InterceptsDialogsInBackgroundActorTab) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  tabs::TabInterface* actor_tab = active_tab();
  content::WebContents* actor_contents = actor_tab->GetContents();
  const ActorSurfaceHandle actor_surface =
      ActorSurfaceHandle::From(actor_tab->GetHandle());

  // Open a foreground tab so `actor_tab` is backgrounded.
  ASSERT_NO_FATAL_FAILURE(OpenForegroundTab(GetTestPageUrl("/title2.html")));
  ASSERT_NE(active_tab(), actor_tab);
  ASSERT_FALSE(actor_tab->IsActivated());

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(
          actor_surface, actor_contents->GetPrimaryMainFrame(),
          "window.alert('bg alert'); window.confirm('bg confirm');")),
      result.GetCallback());
  ExpectOkResult(result);

  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 2u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "bg alert");
  EXPECT_EQ(dialogs[1].type, JavaScriptDialogType::kConfirm);
  EXPECT_EQ(dialogs[1].dialog_message, "bg confirm");

  // Reactivating the background tab should not reveal any queued modal dialog.
  tab_list()->ActivateTab(actor_tab->GetHandle());
  ASSERT_TRUE(
      base::test::RunUntil([actor_tab]() { return actor_tab->IsActivated(); }));
  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          actor_contents);
  ASSERT_TRUE(tab_modal_manager);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
}

// Pausing the actor task restores normal TabModalDialogManager behavior for
// user-facing dialogs, and resuming restores interception.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       PausedTaskDoesNotInterceptDialogs) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  // Establish actor control over the tab.
  {
    ActResultFuture init_result;
    actor_task().Act(
        ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "1 + 1")),
        init_result.GetCallback());
    ExpectOkResult(init_result);
  }

  actor_task().Pause(/*from_actor=*/false);

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  base::RunLoop dialog_shown_loop;
  tab_modal_manager->SetDialogShownCallbackForTesting(
      dialog_shown_loop.QuitClosure());
  content::ExecuteScriptAsync(web_contents(), "window.alert('user dialog')");
  dialog_shown_loop.Run();

  EXPECT_TRUE(tab_modal_manager->IsShowingDialogForTesting());
  const std::u16string empty_prompt;
  EXPECT_TRUE(tab_modal_manager->HandleJavaScriptDialog(
      web_contents(), /*accept=*/true, &empty_prompt));
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  // Resuming the task re-enables interception.
  actor_task().Resume();
  ActResultFuture resumed_result;
  actor_task().Act(ToRequestList(MakeExecuteJsRequest(
                       surface, main_frame(), "window.alert('resumed alert')")),
                   resumed_result.GetCallback());
  ExpectOkResult(resumed_result);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  ASSERT_EQ(resumed_result.Get().size(), 1u);
  const auto& dialogs = resumed_result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "resumed alert");
}

// Intercepts dialogs on headless actor surfaces owned by
// HeadlessWebContentsManager.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       InterceptsDialogsOnHeadlessActorSurface) {
  ScopedHeadlessActorSurface headless =
      actor_keyed_service().CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  content::WebContents* contents = handle.Get()->GetWebContents();
  ASSERT_TRUE(content::NavigateToURL(contents, GetTestPageUrl()));

  base::test::TestFuture<mojom::ActionResultPtr> add_result;
  actor_task().AddActorSurface(handle, /*stop_task_on_detach=*/false,
                               add_result.GetCallback());
  ExpectOkResult(*add_result.Get());

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(
          handle, contents->GetPrimaryMainFrame(),
          "window.alert('headless alert'); window.prompt('headless prompt');")),
      result.GetCallback());
  ExpectOkResult(result);

  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 2u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "headless alert");
  EXPECT_EQ(dialogs[1].type, JavaScriptDialogType::kPrompt);
  EXPECT_EQ(dialogs[1].dialog_message, "headless prompt");
}

// Verifies intercepted dialogs are populated on ExecuteJsResult when building
// the ActionsResult proto response.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       ProtoResultContainsInterceptedDialogs) {
  using ProtoDialog =
      optimization_guide::proto::ExecuteJsResult::JavaScriptDialog;
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  PerformActionsFuture perform_future;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  actor_keyed_service().PerformActions(
      task_id_,
      ToRequestList(MakeExecuteJsRequest(
          surface, main_frame(),
          "window.alert('Waitlist joined'); window.confirm('Continue?');")),
      ActorTaskMetadata(), perform_future.GetCallback());
  ExpectOkResult(perform_future);

  std::vector<ActionResultWithLatencyInfo> cloned_results =
      perform_future.Get();

  base::test::TestFuture<
      base::TimeTicks, std::vector<ActionResultWithLatencyInfo>, TaskId, bool,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>,
      std::unique_ptr<optimization_guide::proto::ActionsResult>,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry>>
      proto_future;
  BuildActionsResultWithObservations(
      *GetProfile(), start_time, std::move(cloned_results), actor_task(),
      /*skip_async_observation_information=*/true, std::nullopt,
      proto_future.GetCallback());

  const std::unique_ptr<optimization_guide::proto::ActionsResult>&
      actions_result = proto_future.Get<5>();
  ASSERT_TRUE(actions_result);
  ASSERT_EQ(actions_result->execute_js_results_size(), 1);
  const auto& execute_js_result = actions_result->execute_js_results(0);
  EXPECT_EQ(execute_js_result.index_of_execute_js_action(), 0);
  ASSERT_EQ(execute_js_result.javascript_dialogs_size(), 2);
  EXPECT_EQ(execute_js_result.javascript_dialogs(0).type(),
            ProtoDialog::TYPE_ALERT);
  EXPECT_EQ(execute_js_result.javascript_dialogs(0).dialog_message(),
            "Waitlist joined");
  EXPECT_EQ(execute_js_result.javascript_dialogs(1).type(),
            ProtoDialog::TYPE_CONFIRM);
  EXPECT_EQ(execute_js_result.javascript_dialogs(1).dialog_message(),
            "Continue?");
  ASSERT_EQ(actions_result->extra_information_size(), 1);
  EXPECT_TRUE(actions_result->extra_information(0).empty());
}

// Verifies intercepted dialogs are still populated on ExecuteJsResult when an
// ExecuteJs action fails without an ExecuteJsResponse (e.g. due to navigation).
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       ProtoResultContainsDialogsOnExecuteJsNavigationFailure) {
  using ProtoDialog =
      optimization_guide::proto::ExecuteJsResult::JavaScriptDialog;
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  PerformActionsFuture perform_future;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  actor_keyed_service().PerformActions(
      task_id_,
      ToRequestList(
          MakeExecuteJsRequest(surface, main_frame(),
                               "window.alert('before nav');"
                               "window.location.href = '/title2.html';"
                               "new Promise(() => {});")),
      ActorTaskMetadata(), perform_future.GetCallback());
  ExpectErrorResult(perform_future,
                    mojom::ActionResultCode::kExecuteJsNavigated);

  ASSERT_EQ(perform_future.Get().size(), 1u);
  EXPECT_FALSE(perform_future.Get()[0].result->execute_js_response);
  EXPECT_EQ(perform_future.Get()[0].tool_name, ExecuteJsToolRequest::kName);

  std::vector<ActionResultWithLatencyInfo> cloned_results =
      perform_future.Get();
  base::test::TestFuture<
      base::TimeTicks, std::vector<ActionResultWithLatencyInfo>, TaskId, bool,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>,
      std::unique_ptr<optimization_guide::proto::ActionsResult>,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry>>
      proto_future;
  BuildActionsResultWithObservations(
      *GetProfile(), start_time, std::move(cloned_results), actor_task(),
      /*skip_async_observation_information=*/true, std::nullopt,
      proto_future.GetCallback());

  const std::unique_ptr<optimization_guide::proto::ActionsResult>&
      actions_result = proto_future.Get<5>();
  ASSERT_TRUE(actions_result);
  ASSERT_EQ(actions_result->execute_js_results_size(), 1);
  const auto& execute_js_result = actions_result->execute_js_results(0);
  EXPECT_EQ(execute_js_result.index_of_execute_js_action(), 0);
  ASSERT_EQ(execute_js_result.javascript_dialogs_size(), 1);
  EXPECT_EQ(execute_js_result.javascript_dialogs(0).type(),
            ProtoDialog::TYPE_ALERT);
  EXPECT_EQ(execute_js_result.javascript_dialogs(0).dialog_message(),
            "before nav");
}

// Dialogs fired between actions (e.g. from a page timer while reflecting) are
// still auto-dismissed without modal UI, and are not falsely attributed to the
// next action.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       DoesNotLeakStaleDialogsFromBetweenActions) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  {
    ActResultFuture init_result;
    actor_task().Act(
        ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "1 + 1")),
        init_result.GetCallback());
    ExpectOkResult(init_result);
  }

  // Trigger a dialog directly on the page while the task is reflecting between
  // turns.
  ASSERT_TRUE(
      content::ExecJs(web_contents(), "window.alert('between turns');"));
  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  ActResultFuture next_result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "2 + 2")),
      next_result.GetCallback());
  ExpectOkResult(next_result);
  ASSERT_EQ(next_result.Get().size(), 1u);
  EXPECT_TRUE(next_result.Get()[0].javascript_dialogs.empty());
}

// Defense-in-depth: if TabModalDialogManager is still holding an open dialog
// while a tab is already under actor control (without going through
// DidTabEnterActorControl / Resume), a subsequent intercepted dialog closes it
// via CloseDialogWithReason(kSubsequentDialogShown) and reports only the actor
// dialog.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       DismissesStaleTabModalDialogOnSubsequentIntercept) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle actor_surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  // Establish actor control over the tab first so the next Act() does not
  // invoke DidTabEnterActorControl() / DismissExistingTabModalDialog().
  {
    ActResultFuture init_result;
    actor_task().Act(
        ToRequestList(MakeExecuteJsRequest(actor_surface, main_frame(), "1")),
        init_result.GetCallback());
    ExpectOkResult(init_result);
  }

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  bool stale_closed = false;
  bool stale_accepted = true;
  bool stale_suppressed = true;
  tab_modal_manager->RunJavaScriptDialog(
      web_contents(), main_frame(), content::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
      u"stale modal dialog", u"",
      base::BindLambdaForTesting([&](bool accept, const std::u16string&) {
        stale_closed = true;
        stale_accepted = accept;
      }),
      &stale_suppressed);
  // Ensure the synthetic dialog is closed before `stale_closed` /
  // `stale_accepted` unwind if an early ASSERT fails.
  base::ScopedClosureRunner cleanup_stale_dialog(base::BindOnce(
      [](javascript_dialogs::TabModalDialogManager* manager) {
        if (manager->IsShowingDialogForTesting()) {
          manager->CloseDialogWithReason(
              javascript_dialogs::TabModalDialogManager::DismissalCause::
                  kCancelDialogsCalled);
        }
      },
      tab_modal_manager));
  ASSERT_FALSE(stale_suppressed);
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());
  ASSERT_FALSE(stale_closed);

  // Trigger a new alert through the interceptor; RunJavaScriptDialog() should
  // defensively dismiss the stale TabModalDialogManager dialog.
  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(actor_surface, main_frame(),
                                         "window.alert('actor alert');")),
      result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(stale_closed);
  EXPECT_FALSE(stale_accepted);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "actor alert");
}

// Observing a non-controlled tab via ObserveActorSurfaceOnce() must not
// dismiss a user's open modal JavaScript dialog on that tab.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       ObserveActorSurfaceOnceDoesNotDismissOpenModalDialog) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  base::RunLoop dialog_shown_loop;
  tab_modal_manager->SetDialogShownCallbackForTesting(
      dialog_shown_loop.QuitClosure());
  content::ExecuteScriptAsync(
      web_contents(),
      "window.confirm_result = window.confirm('user open confirm');");
  dialog_shown_loop.Run();
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  actor_task().ObserveActorSurfaceOnce(surface);
  EXPECT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  EXPECT_TRUE(tab_modal_manager->HandleJavaScriptDialog(
      web_contents(), /*accept=*/true, /*prompt_override=*/nullptr));
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
  EXPECT_EQ(true, content::EvalJs(web_contents(), "window.confirm_result"));
}

// If a foreground tab already has an open modal JS dialog when actor control
// begins, entering actor control dismisses it (failing closed for confirm /
// prompt) so the renderer unblocks and the first actor action does not hang.
IN_PROC_BROWSER_TEST_F(
    JavaScriptDialogInterceptorBrowserTest,
    DismissesPreExistingOpenModalDialogWhenActorControlBegins) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  base::RunLoop dialog_shown_loop;
  tab_modal_manager->SetDialogShownCallbackForTesting(
      dialog_shown_loop.QuitClosure());
  content::ExecuteScriptAsync(
      web_contents(),
      "window.confirm_result = window.confirm('pre-existing confirm');");
  dialog_shown_loop.Run();
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "1 + 1")),
      result.GetCallback());
  ExpectOkResult(result);

  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
  EXPECT_EQ(false, content::EvalJs(web_contents(), "window.confirm_result"));
  ASSERT_EQ(result.Get().size(), 1u);
  ASSERT_TRUE(result.Get()[0].result->execute_js_response);
  EXPECT_EQ(result.Get()[0].result->execute_js_response->result, "2");
  EXPECT_TRUE(result.Get()[0].javascript_dialogs.empty());
}

// Resuming a paused task dismisses any open foreground modal JS dialog left
// behind while the user had control so subsequent actions do not hang.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       DismissesOpenModalDialogWhenTaskResumes) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  {
    ActResultFuture init_result;
    actor_task().Act(
        ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "1 + 1")),
        init_result.GetCallback());
    ExpectOkResult(init_result);
  }

  actor_task().Pause(/*from_actor=*/false);

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  base::RunLoop dialog_shown_loop;
  tab_modal_manager->SetDialogShownCallbackForTesting(
      dialog_shown_loop.QuitClosure());
  content::ExecuteScriptAsync(web_contents(), "window.alert('paused alert');");
  dialog_shown_loop.Run();
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  actor_task().Resume();
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  ActResultFuture resumed_result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "2 + 2")),
      resumed_result.GetCallback());
  ExpectOkResult(resumed_result);
  ASSERT_EQ(resumed_result.Get().size(), 1u);
  ASSERT_TRUE(resumed_result.Get()[0].result->execute_js_response);
  EXPECT_EQ(resumed_result.Get()[0].result->execute_js_response->result, "4");
  EXPECT_TRUE(resumed_result.Get()[0].javascript_dialogs.empty());
}

// Uninterrupting a task that relinquished user control dismisses any open
// foreground modal JS dialog left behind during the interruption.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       DismissesOpenModalDialogOnUninterruptFromUserControl) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  {
    ActResultFuture init_result;
    actor_task().Act(
        ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "1 + 1")),
        init_result.GetCallback());
    ExpectOkResult(init_result);
  }

  actor_task().Interrupt(/*retain_user_control=*/true,
                         ActorTask::InterruptReason::kWaitingUserTakeOver);

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  base::RunLoop dialog_shown_loop;
  tab_modal_manager->SetDialogShownCallbackForTesting(
      dialog_shown_loop.QuitClosure());
  content::ExecuteScriptAsync(web_contents(),
                              "window.alert('interrupted alert');");
  dialog_shown_loop.Run();
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  actor_task().Uninterrupt(ActorTask::State::kReflecting);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "3 + 3")),
      result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  ASSERT_TRUE(result.Get()[0].result->execute_js_response);
  EXPECT_EQ(result.Get()[0].result->execute_js_response->result, "6");
  EXPECT_TRUE(result.Get()[0].javascript_dialogs.empty());
}

// If the user navigates the tab during Interrupt(/*retain_user_control=*/true),
// ActorControlledTabState::PrimaryPageChanged() clears its observed
// WebContents. Uninterrupting must still resolve the surface's WebContents and
// dismiss any open modal dialog on the post-navigation page.
IN_PROC_BROWSER_TEST_F(
    JavaScriptDialogInterceptorBrowserTest,
    DismissesOpenModalDialogOnUninterruptAfterNavigationDuringUserControl) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  {
    ActResultFuture init_result;
    actor_task().Act(
        ToRequestList(MakeExecuteJsRequest(surface, main_frame(), "1 + 1")),
        init_result.GetCallback());
    ExpectOkResult(init_result);
  }

  actor_task().Interrupt(/*retain_user_control=*/true,
                         ActorTask::InterruptReason::kWaitingUserTakeOver);

  // Navigating while interrupted triggers PrimaryPageChanged() while
  // IsUnderActorControl() is false, detaching ActorControlledTabState from
  // `web_contents()`.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetTestPageUrl("/title2.html")));

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  base::RunLoop dialog_shown_loop;
  tab_modal_manager->SetDialogShownCallbackForTesting(
      dialog_shown_loop.QuitClosure());
  content::ExecuteScriptAsync(
      web_contents(),
      "window.confirm_result = window.confirm('post-nav interrupted "
      "confirm');");
  dialog_shown_loop.Run();
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  actor_task().Uninterrupt(ActorTask::State::kReflecting);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
  EXPECT_EQ(false, content::EvalJs(web_contents(), "window.confirm_result"));

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(
          surface, main_frame(), "window.alert('after uninterrupt'); 4 + 4")),
      result.GetCallback());
  ExpectOkResult(result);
  ASSERT_EQ(result.Get().size(), 1u);
  ASSERT_TRUE(result.Get()[0].result->execute_js_response);
  EXPECT_EQ(result.Get()[0].result->execute_js_response->result, "8");
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "after uninterrupt");

  // Pausing after post-navigation Uninterrupt() still removes the interceptor
  // attached to the WebContents.
  EXPECT_NE(JavaScriptDialogInterceptor::FromWebContents(web_contents()),
            nullptr);
  actor_task().Pause(/*from_actor=*/false);
  EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(web_contents()),
            nullptr);
}

// Resuming a paused task dismisses any pending background TabModalDialogManager
// alert immediately, even if no subsequent alert fires before reactivation.
IN_PROC_BROWSER_TEST_F(
    JavaScriptDialogInterceptorBrowserTest,
    DismissesPendingTabModalDialogOnResumeWithoutSubsequentAlert) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  tabs::TabInterface* actor_tab = active_tab();
  content::WebContents* actor_contents = actor_tab->GetContents();
  const ActorSurfaceHandle actor_surface =
      ActorSurfaceHandle::From(actor_tab->GetHandle());

  {
    ActResultFuture init_result;
    actor_task().Act(
        ToRequestList(MakeExecuteJsRequest(
            actor_surface, actor_contents->GetPrimaryMainFrame(), "1 + 1")),
        init_result.GetCallback());
    ExpectOkResult(init_result);
  }

  ASSERT_NO_FATAL_FAILURE(OpenForegroundTab(GetTestPageUrl("/title2.html")));
  ASSERT_FALSE(actor_tab->IsActivated());
  actor_task().Pause(/*from_actor=*/false);
  ASSERT_TRUE(content::ExecJs(actor_contents,
                              "window.alert('stale background alert');"));

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          actor_contents);
  ASSERT_TRUE(tab_modal_manager);
  std::optional<javascript_dialogs::TabModalDialogManager::DismissalCause>
      dismissed_cause;
  tab_modal_manager->SetDialogDismissedCallbackForTesting(
      base::BindLambdaForTesting(
          [&](javascript_dialogs::TabModalDialogManager::DismissalCause cause) {
            dismissed_cause = cause;
          }));

  actor_task().Resume();
  tab_modal_manager->SetDialogDismissedCallbackForTesting(base::NullCallback());
  EXPECT_EQ(dismissed_cause, javascript_dialogs::TabModalDialogManager::
                                 DismissalCause::kCancelDialogsCalled);

  tab_list()->ActivateTab(actor_tab->GetHandle());
  ASSERT_TRUE(
      base::test::RunUntil([actor_tab]() { return actor_tab->IsActivated(); }));
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
}

// Dialogs from a sandboxed opaque-origin iframe (`allow-scripts allow-modals`)
// are still auto-dismissed so they cannot block the renderer, but their text is
// not recorded in `javascript_dialogs` to prevent untrusted prompt injection.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       DoesNotRecordSandboxedIframeDialogs) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(surface, main_frame(), R"JS(
        new Promise(resolve => {
          window.addEventListener('message', event => {
            if (typeof event.data === 'string' &&
                event.data.startsWith('sandbox_done:')) {
              window.alert('main frame alert');
              resolve(event.data);
            }
          });
          const iframe = document.createElement('iframe');
          iframe.sandbox = 'allow-scripts allow-modals';
          iframe.srcdoc =
              '<script>' +
              'window.alert("untrusted sandbox alert");' +
              'const c = window.confirm("untrusted sandbox confirm");' +
              'window.parent.postMessage("sandbox_done:" + c, "*");' +
              '<\/script>';
          document.body.appendChild(iframe);
        })
      )JS")),
      result.GetCallback());
  ExpectOkResult(result);

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  ASSERT_EQ(result.Get().size(), 1u);
  ASSERT_TRUE(result.Get()[0].result->execute_js_response);
  EXPECT_EQ(result.Get()[0].result->execute_js_response->result,
            "\"sandbox_done:false\"");
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "main frame alert");
}

// Dialogs from a non-sandboxed same-origin iframe in the primary page are both
// auto-dismissed and recorded.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       RecordsSameOriginIframeDialogs) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  ActResultFuture result;
  actor_task().Act(
      ToRequestList(MakeExecuteJsRequest(surface, main_frame(), R"JS(
        new Promise(resolve => {
          window.addEventListener('message', event => {
            if (event.data === 'same_origin_done') {
              resolve('ok');
            }
          });
          const iframe = document.createElement('iframe');
          iframe.srcdoc =
              '<script>' +
              'window.alert("same origin iframe alert");' +
              'window.parent.postMessage("same_origin_done", "*");' +
              '<\/script>';
          document.body.appendChild(iframe);
        })
      )JS")),
      result.GetCallback());
  ExpectOkResult(result);

  ASSERT_EQ(result.Get().size(), 1u);
  const auto& dialogs = result.Get()[0].javascript_dialogs;
  ASSERT_EQ(dialogs.size(), 1u);
  EXPECT_EQ(dialogs[0].type, JavaScriptDialogType::kAlert);
  EXPECT_EQ(dialogs[0].dialog_message, "same origin iframe alert");
}

// On a regular actor-controlled tab, beforeunload dialogs continue delegating
// to TabModalDialogManager / AppModalDialogManager rather than being
// intercepted.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       BeforeUnloadDelegatesToAppModalDialogOnRegularTab) {
  const GURL beforeunload_url = GetTestPageUrl("/title1.html");
  const GURL target_url = GetTestPageUrl("/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), beforeunload_url));
  // Register via `window.onbeforeunload` rather than `/actor/beforeunload.html`
  // (which uses an anonymous `addEventListener('beforeunload', ...)` on M159)
  // so the handler can be cleared before browser teardown.
  ASSERT_TRUE(content::ExecJs(web_contents(), R"JS(
    window.onbeforeunload = function(event) {
      event.preventDefault();
      event.returnValue = 'Test';
    };
  )JS"));
  content::PrepContentsForBeforeUnloadTest(web_contents());
  ASSERT_TRUE(base::test::RunUntil([this]() {
    return web_contents()->NeedToFireBeforeUnloadOrUnloadEvents();
  }));

  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());
  base::test::TestFuture<mojom::ActionResultPtr> add_result;
  actor_task().AddActorSurface(surface, /*stop_task_on_detach=*/false,
                               add_result.GetCallback());
  ExpectOkResult(*add_result.Get());
  actor_task().SetState(ActorTask::State::kReflecting);
  ASSERT_NE(JavaScriptDialogInterceptor::MaybeGetForWebContents(web_contents()),
            nullptr);

  web_contents()->GetController().LoadURL(target_url, content::Referrer(),
                                          ::ui::PAGE_TRANSITION_TYPED,
                                          std::string());
  auto* dialog_queue = javascript_dialogs::AppModalDialogQueue::GetInstance();
  ASSERT_TRUE(base::test::RunUntil(
      [dialog_queue]() { return dialog_queue->HasActiveDialog(); }));
  javascript_dialogs::AppModalDialogController* dialog =
      dialog_queue->active_dialog();
  ASSERT_TRUE(dialog);
  EXPECT_TRUE(dialog->is_before_unload_dialog());
  dialog->view()->CancelAppModalDialog();

  // Clear `window.onbeforeunload` so browser teardown is not blocked and wait
  // for the renderer's cancelled beforeunload reply to settle.
  ASSERT_TRUE(content::ExecJs(web_contents(), "window.onbeforeunload = null;"));
  ASSERT_TRUE(base::test::RunUntil([this]() {
    return !web_contents()->NeedToFireBeforeUnloadOrUnloadEvents();
  }));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), beforeunload_url);
}

// A multi-action Act([ExecuteJs, Click, ExecuteJs]) isolates intercepted
// dialogs per action and only copies ExecuteJs actions (with their original
// action indices) into ActionsResult.execute_js_results.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorBrowserTest,
                       MultiActionActIsolatesDialogsAndIndexesProto) {
  using ProtoDialog =
      optimization_guide::proto::ExecuteJsResult::JavaScriptDialog;
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      GetTestPageUrl("/actor/page_with_clickable_element.html")));
  ASSERT_TRUE(content::ExecJs(web_contents(), R"JS(
    document.getElementById('clickable').onclick = () => {
      window.alert('from click');
    };
  )JS"));
  std::optional<int> button_id =
      content::GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());
  PerformActionsFuture perform_future;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  actor_keyed_service().PerformActions(
      task_id_,
      ToRequestList(MakeExecuteJsRequest(surface, main_frame(),
                                         "window.alert('first js'); 1"),
                    MakeClickRequest(*main_frame(), *button_id),
                    MakeExecuteJsRequest(surface, main_frame(),
                                         "window.alert('second js'); 2")),
      ActorTaskMetadata(), perform_future.GetCallback());
  ExpectOkResult(perform_future);

  const auto& results = perform_future.Get();
  ASSERT_EQ(results.size(), 3u);
  ASSERT_EQ(results[0].javascript_dialogs.size(), 1u);
  EXPECT_EQ(results[0].javascript_dialogs[0].dialog_message, "first js");
  ASSERT_EQ(results[1].javascript_dialogs.size(), 1u);
  EXPECT_EQ(results[1].javascript_dialogs[0].dialog_message, "from click");
  ASSERT_EQ(results[2].javascript_dialogs.size(), 1u);
  EXPECT_EQ(results[2].javascript_dialogs[0].dialog_message, "second js");

  std::vector<ActionResultWithLatencyInfo> cloned_results = results;
  base::test::TestFuture<
      base::TimeTicks, std::vector<ActionResultWithLatencyInfo>, TaskId, bool,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>,
      std::unique_ptr<optimization_guide::proto::ActionsResult>,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry>>
      proto_future;
  BuildActionsResultWithObservations(
      *GetProfile(), start_time, std::move(cloned_results), actor_task(),
      /*skip_async_observation_information=*/true, std::nullopt,
      proto_future.GetCallback());

  const std::unique_ptr<optimization_guide::proto::ActionsResult>&
      actions_result = proto_future.Get<5>();
  ASSERT_TRUE(actions_result);
  ASSERT_EQ(actions_result->execute_js_results_size(), 2);

  const auto& first_proto = actions_result->execute_js_results(0);
  EXPECT_EQ(first_proto.index_of_execute_js_action(), 0);
  EXPECT_EQ(first_proto.result(), "1");
  ASSERT_EQ(first_proto.javascript_dialogs_size(), 1);
  EXPECT_EQ(first_proto.javascript_dialogs(0).type(), ProtoDialog::TYPE_ALERT);
  EXPECT_EQ(first_proto.javascript_dialogs(0).dialog_message(), "first js");

  const auto& second_proto = actions_result->execute_js_results(1);
  EXPECT_EQ(second_proto.index_of_execute_js_action(), 2);
  EXPECT_EQ(second_proto.result(), "2");
  ASSERT_EQ(second_proto.javascript_dialogs_size(), 1);
  EXPECT_EQ(second_proto.javascript_dialogs(0).type(), ProtoDialog::TYPE_ALERT);
  EXPECT_EQ(second_proto.javascript_dialogs(0).dialog_message(), "second js");
}

class JavaScriptDialogInterceptorDisabledBrowserTest : public ActorToolsTest {
 public:
  JavaScriptDialogInterceptorDisabledBrowserTest() {
    // Enable `kActorExecuteJsTool` while disabling
    // `kActorInterceptJavaScriptDialogs` to verify dialog interception is
    // controlled independently of ExecuteJs.
    features_.InitWithFeatures({features::kGlicActor, kActorExecuteJsTool},
                               {kActorInterceptJavaScriptDialogs});
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList features_;
};

// When kActorInterceptJavaScriptDialogs is disabled (even if
// kActorExecuteJsTool is enabled), pre-existing modal dialogs remain open
// across AddActorSurface() and Resume(), and dialogs on an actor-controlled tab
// continue delegating to TabModalDialogManager.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorDisabledBrowserTest,
                       DisablesInterception) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  auto open_modal_alert = [&](std::string_view script) {
    base::RunLoop dialog_shown_loop;
    tab_modal_manager->SetDialogShownCallbackForTesting(
        dialog_shown_loop.QuitClosure());
    content::ExecuteScriptAsync(web_contents(), script);
    dialog_shown_loop.Run();
  };

  // A pre-existing modal dialog opened before actor control begins must remain
  // open across AddActorSurface() / DismissExistingTabModalDialog().
  open_modal_alert("window.alert('pre-existing alert');");
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  base::test::TestFuture<mojom::ActionResultPtr> add_result;
  actor_task().AddActorSurface(surface, /*stop_task_on_detach=*/false,
                               add_result.GetCallback());
  ExpectOkResult(*add_result.Get());
  actor_task().SetState(ActorTask::State::kReflecting);

  EXPECT_TRUE(tab_modal_manager->IsShowingDialogForTesting());
  EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(web_contents()),
            nullptr);
  EXPECT_TRUE(tab_modal_manager->HandleJavaScriptDialog(
      web_contents(), /*accept=*/true, /*prompt_override=*/nullptr));
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  // New dialogs fired while under actor control still delegate to
  // TabModalDialogManager.
  open_modal_alert("window.alert('killswitch alert');");
  EXPECT_TRUE(tab_modal_manager->IsShowingDialogForTesting());
  EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(web_contents()),
            nullptr);
  EXPECT_TRUE(tab_modal_manager->HandleJavaScriptDialog(
      web_contents(), /*accept=*/true, /*prompt_override=*/nullptr));
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());

  // A modal dialog opened while paused must also remain open across Resume() /
  // DismissExistingTabModalDialog().
  actor_task().Pause(/*from_actor=*/false);
  open_modal_alert("window.alert('paused alert');");
  ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());

  actor_task().Resume();
  EXPECT_TRUE(tab_modal_manager->IsShowingDialogForTesting());
  EXPECT_EQ(JavaScriptDialogInterceptor::FromWebContents(web_contents()),
            nullptr);
  EXPECT_TRUE(tab_modal_manager->HandleJavaScriptDialog(
      web_contents(), /*accept=*/true, /*prompt_override=*/nullptr));
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
}

class JavaScriptDialogInterceptorFieldTrialBrowserTest : public ActorToolsTest {
 public:
  JavaScriptDialogInterceptorFieldTrialBrowserTest() {
    features_.InitWithFeaturesAndParameters(
        {{features::kGlicActor, {}}, {kActorInterceptJavaScriptDialogs, {}}},
        {});
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Non-actor Chrome dialogs and dialog-free actor control transitions on a real
// tab (with an idle TabModalDialogManager) must not prematurely activate the
// `kActorInterceptJavaScriptDialogs` Finch field trial.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogInterceptorFieldTrialBrowserTest,
                       DoesNotActivateFieldTrialUntilActorDialog) {
  base::FieldTrial* trial =
      base::FeatureList::GetFieldTrial(kActorInterceptJavaScriptDialogs);
  ASSERT_TRUE(trial);
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  auto* tab_modal_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(tab_modal_manager);

  // Regular non-actor JS dialog must not activate
  // `kActorInterceptJavaScriptDialogs`.
  {
    base::RunLoop dialog_shown_loop;
    tab_modal_manager->SetDialogShownCallbackForTesting(
        dialog_shown_loop.QuitClosure());
    content::ExecuteScriptAsync(web_contents(),
                                "window.alert('non-actor alert');");
    dialog_shown_loop.Run();
    ASSERT_TRUE(tab_modal_manager->IsShowingDialogForTesting());
    EXPECT_TRUE(tab_modal_manager->HandleJavaScriptDialog(
        web_contents(), /*accept=*/true, /*prompt_override=*/nullptr));
    EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
  }
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  // Entering actor control on a tab with an idle TabModalDialogManager must
  // not activate `kActorInterceptJavaScriptDialogs` either.
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());
  base::test::TestFuture<mojom::ActionResultPtr> add_result;
  actor_task().AddActorSurface(surface, /*stop_task_on_detach=*/false,
                               add_result.GetCallback());
  ExpectOkResult(*add_result.Get());
  actor_task().SetState(ActorTask::State::kReflecting);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  // Firing a JS dialog while under actor control queries the feature flag,
  // activates the trial, and intercepts the dialog without modal UI.
  ASSERT_TRUE(content::ExecJs(web_contents(), "window.alert('actor alert');"));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
  EXPECT_FALSE(tab_modal_manager->IsShowingDialogForTesting());
}

}  // namespace
}  // namespace actor
