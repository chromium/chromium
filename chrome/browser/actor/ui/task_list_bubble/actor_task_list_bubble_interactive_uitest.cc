// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/ui/actor_ui_interactive_browser_test.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"

class ActorTaskListBubbleInteractiveUiTest
    : public ActorUiInteractiveBrowserTest {
 public:
  ActorTaskListBubbleInteractiveUiTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kGlicRollout, {}},
            {features::kGlicActor,
             {{features::kGlicActorPolicyControlExemption.name, "true"}}},
            {features::kGlicActorUi,
             {{features::kGlicActorUiTaskIconName, "true"}}},
        },
        {});
  }

 protected:
  glic::GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorTaskListBubbleInteractiveUiTest,
                       ShowBubbleWithTask) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  const char kFirstTaskItem[] = "FirstTaskItem";
  StartActingOnTab();
  RunTestSequence(
      Do([&]() { PauseTask(); }),
      InAnyContext(WaitForShow(kGlicActorTaskIconElementId)),
      InAnyContext(WaitForShow(kActorTaskListBubbleView)),
      CheckView(
          kActorTaskListBubbleView,
          [](views::View* view) { return view->children().size() == 1u; }),
      InSameContext(NameDescendantViewByType<ActorTaskListBubbleRowButton>(
          kActorTaskListBubbleView, kFirstTaskItem, 0)),
      CheckViewProperty(
          kFirstTaskItem, &ActorTaskListBubbleRowButton::GetSubtitleText,
          l10n_util::GetStringUTF16(
              IDS_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE)),
      CheckView(
          kFirstTaskItem,
          [](ActorTaskListBubbleRowButton* button) {
            return button->GetRedirectIconForTesting()->GetTooltipText() ==
                   l10n_util::GetStringUTF16(IDS_TAB_SEARCH_A11Y_OPEN_TAB);
          }),
      PressButton(kFirstTaskItem),
      InAnyContext(WaitForHide(kActorTaskListBubbleView)));
}

IN_PROC_BROWSER_TEST_F(ActorTaskListBubbleInteractiveUiTest,
                       ClickingOnTaskInBubbleActuatesTab) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto* tab_one = browser()->GetActiveTabInterface();
  StartActingOnTab();

  // Add and activate the non-actuation tab.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1,
                                     chrome::ChromeUINewTabURLAsGURL(),
                                     ui::PAGE_TRANSITION_LINK));
  auto* tab_two = browser()->GetTabStripModel()->GetTabAtIndex(1);
  browser()->GetTabStripModel()->ActivateTabAt(1);
  EXPECT_FALSE(tab_one->IsActivated());
  EXPECT_TRUE(tab_two->IsActivated());

  const char kFirstTaskItem[] = "FirstTaskItem";
  base::UserActionTester user_action_tester;
  RunTestSequence(
      Do([&]() { PauseTask(); }),
      InAnyContext(WaitForShow(kGlicActorTaskIconElementId)),
      InAnyContext(WaitForShow(kActorTaskListBubbleView)),
      CheckView(
          kActorTaskListBubbleView,
          [](views::View* view) { return view->children().size() == 1u; }),
      InSameContext(NameDescendantViewByType<ActorTaskListBubbleRowButton>(
          kActorTaskListBubbleView, kFirstTaskItem, 0)),
      CheckViewProperty(
          kFirstTaskItem, &ActorTaskListBubbleRowButton::GetSubtitleText,
          l10n_util::GetStringUTF16(
              IDS_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE)),
      PressButton(kFirstTaskItem),
      InAnyContext(WaitForHide(kActorTaskListBubbleView)));

  EXPECT_TRUE(tab_one->IsActivated());
  EXPECT_FALSE(tab_two->IsActivated());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.TaskListBubble.Row.Click"));
}

IN_PROC_BROWSER_TEST_F(ActorTaskListBubbleInteractiveUiTest,
                       StopTransientTaskDoesNotShowBubble) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  StartActingOnTab(actor::webui::mojom::TaskDuration::kTransient);
  RunTestSequence(
      InAnyContext(WaitForShow(kGlicActorTaskIconElementId)),
      Do([&]() { CompleteTask(); }),
      // Nudge text should be updated, but bubble should not be shown.
      Check([]() { return true; }),
      InAnyContext(EnsureNotPresent(kActorTaskListBubbleView)));
}

IN_PROC_BROWSER_TEST_F(ActorTaskListBubbleInteractiveUiTest,
                       ExperimentalTriggeringTaskShowsBubbleInActingState) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto task_options = actor::webui::mojom::TaskOptions::New();
  task_options->duration = actor::webui::mojom::TaskDuration::kDefault;
  task_options->feature_mode =
      glic::mojom::FeatureMode::kExperimentalTriggering;
  actor::TaskId task_id = actor_keyed_service()->CreateTaskWithOptions(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker(),
      std::move(task_options), nullptr);
  base::test::TestFuture<actor::mojom::ActionResultPtr> future;
  actor_keyed_service()->GetTask(task_id)->AddTab(
      browser()->GetActiveTabInterface()->GetHandle(),
      /*stop_task_on_detach=*/true, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  actor::ExpectOkResult(future);

  // Transition the task to kActing state manually to trigger the bubble show
  actor_keyed_service()->GetTask(task_id)->SetState(
      actor::ActorTask::State::kActing);
  // Transitioning to `kActing` triggers an asynchronous animation/layout task
  // to collapse the Glic button. `InAnyContext(WaitForShow)` is not sufficient
  // here because if the test checks the UI before the layout task runs, the
  // wide Glic button will cause FlexLayout to temporarily hide the task icon
  // to prevent overflow. `base::test::RunUntil` is used here to wait until the
  // task icon is both shown and remains visible after layout settling.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    views::View* icon_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kGlicActorTaskIconElementId,
            BrowserView::GetBrowserViewForBrowser(browser())
                ->GetElementContext());
    return icon_view && icon_view->GetVisible();
  }));

  const char kFirstTaskItem[] = "FirstTaskItem";
  RunTestSequence(
      InAnyContext(WaitForShow(kGlicActorTaskIconElementId)),
      InAnyContext(WaitForShow(kActorTaskListBubbleView)),
      CheckView(
          kActorTaskListBubbleView,
          [](views::View* view) { return view->children().size() == 1u; }),
      InSameContext(NameDescendantViewByType<ActorTaskListBubbleRowButton>(
          kActorTaskListBubbleView, kFirstTaskItem, 0)),
      PressButton(kFirstTaskItem),
      InAnyContext(WaitForHide(kActorTaskListBubbleView)));
}

// Unlike the other tests in this file, this test suite uses the Glic actor test
// fixture in order to test interactions with Glic instances / web clients.
class GlicActorTaskListBubbleInteractiveUiTest
    : public glic::test::GlicActorUiTest {
 public:
  GlicActorTaskListBubbleInteractiveUiTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi, {{features::kGlicActorUiTaskIconName, "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorTaskListBubbleInteractiveUiTest,
                       NotificationReceivedAfterClickingOnTaskInBubble) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const char kFirstTaskItem[] = "FirstTaskItem";

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      // Pause the task.
      PauseActorTask(),
      // Wait for bubble.
      InAnyContext(WaitForShow(kActorTaskListBubbleView)),
      // Check bubble content.
      CheckView(
          kActorTaskListBubbleView,
          [](views::View* view) { return view->children().size() == 1u; }),
      InSameContext(NameDescendantViewByType<ActorTaskListBubbleRowButton>(
          kActorTaskListBubbleView, kFirstTaskItem, 0)),
      CheckViewProperty(
          kFirstTaskItem, &ActorTaskListBubbleRowButton::GetSubtitleText,
          l10n_util::GetStringUTF16(
              IDS_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE)),
      // Set up a promise to listen for the row clicked event.
      ExecuteInGlic(base::BindLambdaForTesting(
          [](content::WebContents* glic_web_contents) {
            ASSERT_TRUE(content::ExecJs(glic_web_contents, R"js(
              (() => {
                window.buttonClickedPromise = new Promise(resolve => {
                  client.browser.actorTaskListRowClicked().subscribe(resolve);
                });
              })();
            )js"));
          })),
      // Click the row item.
      PressButton(kFirstTaskItem),
      // Wait for bubble hide.
      InAnyContext(WaitForHide(kActorTaskListBubbleView)),
      // Verify kNewActorTabId is still active.
      Check([this]() {
        return browser()->tab_strip_model()->IsTabInForeground(
            browser()->tab_strip_model()->GetIndexOfWebContents(
                tab_handle_.Get()->GetContents()));
      }),
      // Verify the row clicked event was received by the web client.
      ExecuteInGlic(base::BindLambdaForTesting(
          [this](content::WebContents* glic_web_contents) {
            EXPECT_EQ(content::EvalJs(glic_web_contents, R"js(
              window.buttonClickedPromise
            )js")
                          .ExtractInt(),
                      task_id_.value());
          })));
}
