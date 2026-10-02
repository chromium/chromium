// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/actor_task_unload_handler.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace actor::ui {
namespace {

using actor::mojom::ActionResultPtr;
using base::test::TestFuture;

class BaseActorUiTabControllerTest : public InProcessBrowserTest {
 public:
  BaseActorUiTabControllerTest() = default;
  ~BaseActorUiTabControllerTest() override = default;

 protected:
  views::AnimatedImageView* GetSpinner() {
    TabStripRegionView* tab_strip_view =
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view();
    tabs::TabInterface* active_tab =
        browser()->GetTabStripModel()->GetActiveTab();
    if (!active_tab) {
      return nullptr;
    }
    views::View* tab_specific =
        tab_strip_view->GetTabAnchorView(active_tab->GetHandle());
    if (!tab_specific) {
      return nullptr;
    }
    views::AnimatedImageView* spinner =
        views::AsViewClass<AlertIndicatorButton>(
            tab_specific->GetViewByElementId(kTabAlertIndicatorButtonElementId))
            ->GetActorIndicatorSpinnerForTesting();
    return spinner;
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->GetProfile());
  }

  base::test::ScopedFeatureList feature_list_;
};

class ActorUiTabControllerTest : public BaseActorUiTabControllerTest {
 public:
  ActorUiTabControllerTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicActorUi,
          {{features::kGlicActorUiTabIndicator.name, "true"}}},
         {features::kGlicActorUiTabIndicatorSpinnerIgnoreReducedMotion, {}}},
        {});
  }
  ~ActorUiTabControllerTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerTest,
                       TabIndicatorVisibleDuringActuation) {
  Profile* const profile = browser()->GetProfile();
  ActorUiStateManager* state_manager = ActorUiStateManager::Get(profile);
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_NE(tab, nullptr);
  ActorUiTabControllerInterface* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  // Initially, the indicator should not be visible.
  tabs::TabAlertController* const tab_alert_controller =
      tabs::TabAlertController::From(tab);
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner(), nullptr);

  // Start acting on the tab.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  ASSERT_TRUE(result.Wait());
  actor::ExpectOkResult(result);

  // The indicator should now be visible.
  EXPECT_TRUE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  ASSERT_NE(GetSpinner(), nullptr);
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kPlaying);
  EXPECT_TRUE(GetSpinner()->GetVisible());
  EXPECT_FALSE(GetSpinner()->bounds().IsEmpty());

  // Stop acting on the tab.
  state_manager->OnUiEvent(StoppedActingOnTab(tab->GetHandle()));

  // The indicator should be hidden again.
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kStopped);
}

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerTest,
                       TabSpinnerNotVisibleWhenWaitingOnUser) {
  // Start task on tab.
  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  actor::TaskId task_id = actor_service->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_service->GetTask(task_id);
  actor::ui::StartTask start_task_event(task_id);
  ActorUiStateManager::Get(browser()->GetProfile())
      ->OnUiEvent(start_task_event);
  // Need to wait for the AUSM to notify the GlicActivityManager.
  base::PlatformThread::Sleep(actor::ui::kProfileScopedUiUpdateDebounceDelay);

  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0,
                                     chrome::ChromeUINewTabURLAsGURL(),
                                     ::ui::PAGE_TRANSITION_LINK));
  auto* tab_one = browser()->GetTabStripModel()->GetTabAtIndex(0);
  base::RunLoop loop;
  task->AddTab(
      tab_one->GetHandle(),
      /*stop_task_on_detach=*/true,
      base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
        EXPECT_TRUE(actor::IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  tabs::TabAlertController* const tab_alert_controller =
      tabs::TabAlertController::From(tab_one);

  // The indicator should be visible on the actuating tab.
  EXPECT_TRUE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  ASSERT_NE(GetSpinner(), nullptr);
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kPlaying);
  EXPECT_TRUE(GetSpinner()->GetVisible());
  EXPECT_FALSE(GetSpinner()->bounds().IsEmpty());

  // Wait for user event.
  ActorUiStateManager::Get(browser()->GetProfile())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kWaitingOnUser));
  // Need to wait for the AUSM to notify the GlicActivityManager.
  base::PlatformThread::Sleep(actor::ui::kProfileScopedUiUpdateDebounceDelay);

  // The static icon should be visible, but not the spinner.
  EXPECT_TRUE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorWaitingOnUser));
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kStopped);

  // Restart the task
  ActorUiStateManager::Get(browser()->GetProfile())
      ->OnUiEvent(actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kActing));
  // Need to wait for the AUSM to notify the GlicActivityManager.
  base::PlatformThread::Sleep(actor::ui::kProfileScopedUiUpdateDebounceDelay);

  // State should return to before WaitingOnUser
  EXPECT_TRUE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  ASSERT_NE(GetSpinner(), nullptr);
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kPlaying);
  EXPECT_TRUE(GetSpinner()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerTest,
                       RecordsUserActionOnActiveStatusChange) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank?1"),
                            ::ui::PageTransition::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* actuating_tab =
      browser()->GetTabStripModel()->GetActiveTab();

  // Start acting on the tab.
  base::RunLoop loop;
  actor_keyed_service()->GetTask(task_id)->AddTab(
      actuating_tab->GetHandle(),
      /*stop_task_on_detach=*/true,
      base::BindLambdaForTesting([&](ActionResultPtr result) {
        EXPECT_TRUE(IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  base::UserActionTester user_action_tester;
  // Add a new tab and make actuating tab active to trigger the active status
  // change.
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank?2"),
                            ::ui::PageTransition::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(
      browser()->GetTabStripModel()->GetIndexOfTab(actuating_tab));

  // The UserAction should record the active status change.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.ActuatingTabWebContentsAttached"));

  // Stop acting on the tab.
  actor_keyed_service()->GetTask(task_id)->RemoveTab(
      actuating_tab->GetHandle());

  // The UserAction shouldn't record any further changes if we reactivate the
  // previously actuating tab.
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank?3"),
                            ::ui::PageTransition::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(
      browser()->GetTabStripModel()->GetIndexOfTab(actuating_tab));

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.ActuatingTabWebContentsAttached"));
}

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerTest,
                       TabAlertControllerNotifiedOnUpdate) {
  Profile* const profile = browser()->GetProfile();
  ActorUiStateManager* state_manager = ActorUiStateManager::Get(profile);
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_NE(tab, nullptr);

  tabs::TabAlertController* const tab_alert_controller =
      tabs::TabAlertController::From(tab);
  base::test::TestFuture<std::optional<tabs::TabAlert>> alert_future;
  base::CallbackListSubscription subscription =
      tab_alert_controller->AddAlertToShowChangedCallback(
          alert_future.GetRepeatingCallback());

  // The alert controller should notify when the indicator is shown.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  actor::ExpectOkResult(result);

  EXPECT_EQ(alert_future.Take(), tabs::TabAlert::kActorAccessing);

  // The alert controller should also notify when the indicator is hidden.
  state_manager->OnUiEvent(StoppedActingOnTab(tab->GetHandle()));
  EXPECT_EQ(alert_future.Take(), std::nullopt);
}

class ActorUiTabControllerDisabledTest : public BaseActorUiTabControllerTest {
 public:
  ActorUiTabControllerDisabledTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiTabIndicator.name, "false"}});
  }
  ~ActorUiTabControllerDisabledTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerDisabledTest,
                       TabIndicatorNotVisibleWhenFeatureDisabled) {
  Profile* const profile = browser()->GetProfile();
  ActorUiStateManager* state_manager = ActorUiStateManager::Get(profile);
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_NE(tab, nullptr);
  ActorUiTabControllerInterface* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  // Initially, the indicator should not be visible.
  tabs::TabAlertController* const tab_alert_controller =
      tabs::TabAlertController::From(tab);
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner(), nullptr);
  // Start acting on the tab.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  actor::ExpectOkResult(result);

  // The indicator should still not be visible.
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner(), nullptr);
}

class ActorUiTabIndicatorSpinnerIgnoreReducedMotionDisabled
    : public BaseActorUiTabControllerTest {
 public:
  ActorUiTabIndicatorSpinnerIgnoreReducedMotionDisabled() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicActorUi,
          {{features::kGlicActorUiTabIndicator.name, "true"}}}},
        {features::kGlicActorUiTabIndicatorSpinnerIgnoreReducedMotion});
  }
  ~ActorUiTabIndicatorSpinnerIgnoreReducedMotionDisabled() override = default;
};

IN_PROC_BROWSER_TEST_F(ActorUiTabIndicatorSpinnerIgnoreReducedMotionDisabled,
                       TabIndicatorVisibleDuringActuation) {
  Profile* const profile = browser()->GetProfile();
  ActorUiStateManager* state_manager = ActorUiStateManager::Get(profile);
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_NE(tab, nullptr);
  ActorUiTabControllerInterface* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  // Initially, the indicator should not be visible.
  tabs::TabAlertController* const tab_alert_controller =
      tabs::TabAlertController::From(tab);
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner(), nullptr);

  // Start acting on the tab.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  ASSERT_TRUE(result.Wait());
  actor::ExpectOkResult(result);

  // The indicator should now be visible.
  EXPECT_TRUE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  ASSERT_NE(GetSpinner(), nullptr);
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kPlaying);
  EXPECT_TRUE(GetSpinner()->GetVisible());
  EXPECT_FALSE(GetSpinner()->bounds().IsEmpty());
  EXPECT_FALSE(GetSpinner()
                   ->animated_image()
                   ->GetPlaybackConfig()
                   ->ignore_reduced_motion);

  // Stop acting on the tab.
  state_manager->OnUiEvent(StoppedActingOnTab(tab->GetHandle()));

  // The indicator should be hidden again.
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kStopped);
}

class ActorUiTabControllerNavigationConfirmTest
    : public BaseActorUiTabControllerTest {
 public:
  ActorUiTabControllerNavigationConfirmTest() {
    feature_list_.InitWithFeatures(
        {features::kGlicConfirmTabClose, features::kGlicActor,
         features::kGlicActorUi},
        {});
  }
  ~ActorUiTabControllerNavigationConfirmTest() override = default;

  void SetUpOnMainThread() override {
    BaseActorUiTabControllerTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    // These tests exercise the confirmation dialog itself, so opt in
    // explicitly instead of inheriting the process global suppression state.
    previous_suppress_confirm_dialog_ =
        ActorTaskTabCloseConfirmDialog::ShouldSuppressForTesting();
    ActorTaskTabCloseConfirmDialog::SetSuppressForTesting(false);
  }

  void TearDownOnMainThread() override {
    ActorTaskTabCloseConfirmDialog::SetSuppressForTesting(
        previous_suppress_confirm_dialog_);
    BaseActorUiTabControllerTest::TearDownOnMainThread();
  }

 protected:
  // Creates a task and puts every tab in `tabs` under its control. Returns a
  // null TaskId if the task could not be created.
  actor::TaskId StartTaskOnTabs(std::vector<tabs::TabInterface*> tabs,
                                bool stop_task_on_detach = false) {
    actor::TaskId task_id = actor_keyed_service()->CreateTask(
        actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
    actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
    if (!task) {
      ADD_FAILURE() << "Failed to create the actor task.";
      return actor::TaskId();
    }
    for (tabs::TabInterface* tab : tabs) {
      TestFuture<ActionResultPtr> add_tab_future;
      task->AddTab(tab->GetHandle(), stop_task_on_detach,
                   add_tab_future.GetCallback());
      EXPECT_TRUE(add_tab_future.Wait());
      actor::ExpectOkResult(add_tab_future);
    }
    return task_id;
  }

  // Starts the kind of browser initiated navigation the omnibox (TYPED) or the
  // bookmark bar (AUTO_BOOKMARK) produces.
  void NavigateViaUserUi(tabs::TabInterface* tab,
                         const GURL& url,
                         ::ui::PageTransition transition) {
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = transition;
    params.is_renderer_initiated = false;
    tab->GetContents()->GetController().LoadURLWithParams(params);
  }

  views::Widget* WaitForConfirmDialog(ActorUiTabController* controller) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return controller->GetActiveNavigationConfirmDialogWidgetForTesting() !=
             nullptr;
    }));
    return controller->GetActiveNavigationConfirmDialogWidgetForTesting();
  }

  // Returns the Actor tab close confirmation shown by the browser's
  // UnloadController, or nullptr if none is showing.
  views::Widget* GetTabCloseConfirmDialog() {
    for (const auto& handler :
         UnloadController::From(browser())->tab_unload_handlers_for_testing()) {
      if (auto* widget = static_cast<ActorTaskUnloadHandler*>(handler.get())
                             ->GetActiveDialogWidgetForTesting()) {
        return widget;
      }
    }
    return nullptr;
  }

  static ::ui::PageTransition OmniboxTransition() {
    return ::ui::PageTransitionFromInt(::ui::PAGE_TRANSITION_TYPED |
                                       ::ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  }

 private:
  bool previous_suppress_confirm_dialog_ = false;
};

// Uses the omnibox submission path (SetUserText() plus OpenCurrentSelection())
// so this also checks that an omnibox commit raises the confirmation.
IN_PROC_BROWSER_TEST_F(ActorUiTabControllerNavigationConfirmTest,
                       UserUiNavigation_CancelKeepsTaskActive) {
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ActorUiTabController* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  const GURL destination_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), initial_url));

  actor::TaskId task_id = StartTaskOnTabs({tab});

  ui_test_utils::SendToOmniboxAndSubmit(browser(), destination_url.spec());

  views::Widget* dialog = WaitForConfirmDialog(controller);
  ASSERT_NE(dialog, nullptr);
  dialog->widget_delegate()->AsDialogDelegate()->CancelDialog();

  EXPECT_EQ(tab->GetContents()->GetLastCommittedURL(), initial_url);
  EXPECT_NE(actor_keyed_service()->GetTask(task_id), nullptr);

  actor_keyed_service()->StopTask(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
}

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerNavigationConfirmTest,
                       UserUiNavigation_AcceptStopsTaskAndNavigates) {
  base::HistogramTester histogram_tester;
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ActorUiTabController* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  const GURL destination_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), initial_url));

  actor::TaskId task_id = StartTaskOnTabs({tab});

  // Simulate a bookmark click by the user.
  NavigateViaUserUi(tab, destination_url, ::ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  views::Widget* dialog = WaitForConfirmDialog(controller);
  ASSERT_NE(dialog, nullptr);
  dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();

  ASSERT_TRUE(content::WaitForLoadStop(tab->GetContents()));
  EXPECT_EQ(tab->GetContents()->GetLastCommittedURL(), destination_url);
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id), nullptr);
  histogram_tester.ExpectUniqueSample(
      "Actor.Task.StoppedReason",
      actor::ActorTask::StoppedReason::kUserNavigatedAway, 1);
}

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerNavigationConfirmTest,
                       UserUiNavigation_DuplicateNavigationWhileDialogOpen) {
  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ActorUiTabController* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  const GURL first_destination_url =
      embedded_test_server()->GetURL("/title2.html");
  const GURL second_destination_url =
      embedded_test_server()->GetURL("/title3.html");
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), initial_url));

  actor::TaskId task_id = StartTaskOnTabs({tab});

  NavigateViaUserUi(tab, first_destination_url, OmniboxTransition());
  views::Widget* first_dialog = WaitForConfirmDialog(controller);
  ASSERT_NE(first_dialog, nullptr);

  // A second navigation replaces the dialog: the first one is destroyed (which
  // cancels only the first navigation) and a new dialog is shown.
  views::test::WidgetDestroyedWaiter destroyed_waiter(first_dialog);
  NavigateViaUserUi(tab, second_destination_url,
                    ::ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  destroyed_waiter.Wait();

  views::Widget* second_dialog = WaitForConfirmDialog(controller);
  ASSERT_NE(second_dialog, nullptr);
  second_dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();

  // Accepting resolves the second navigation, not the cancelled first one.
  ASSERT_TRUE(content::WaitForLoadStop(tab->GetContents()));
  EXPECT_EQ(tab->GetContents()->GetLastCommittedURL(), second_destination_url);
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id), nullptr);
}

// A task controlling two tabs shows the confirmation on the tab that is
// navigating, and not on the other tab.
IN_PROC_BROWSER_TEST_F(ActorUiTabControllerNavigationConfirmTest,
                       UserUiNavigation_ConfirmsOnNavigatingTabOnly) {
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  const GURL destination_url = embedded_test_server()->GetURL("/title2.html");

  tabs::TabInterface* tab_a = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(tab_a->GetContents(), initial_url));
  ASSERT_TRUE(AddTabAtIndex(1, initial_url,
                            ::ui::PageTransition::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* tab_b = browser()->GetTabStripModel()->GetTabAtIndex(1);
  ASSERT_NE(tab_a, tab_b);

  ActorUiTabController* controller_a = ActorUiTabController::From(tab_a);
  ActorUiTabController* controller_b = ActorUiTabController::From(tab_b);
  ASSERT_NE(controller_a, nullptr);
  ASSERT_NE(controller_b, nullptr);

  actor::TaskId task_id = StartTaskOnTabs({tab_a, tab_b});
  browser()->GetTabStripModel()->ActivateTabAt(
      browser()->GetTabStripModel()->GetIndexOfTab(tab_a));

  NavigateViaUserUi(tab_a, destination_url, OmniboxTransition());

  views::Widget* dialog = WaitForConfirmDialog(controller_a);
  ASSERT_NE(dialog, nullptr);
  EXPECT_EQ(controller_b->GetActiveNavigationConfirmDialogWidgetForTesting(),
            nullptr);
  EXPECT_EQ(browser()->GetTabStripModel()->GetActiveTab(), tab_a);

  dialog->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_EQ(tab_a->GetContents()->GetLastCommittedURL(), initial_url);
  EXPECT_NE(actor_keyed_service()->GetTask(task_id), nullptr);

  actor_keyed_service()->StopTask(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
}

// Closing the tab while the navigation confirmation is open supersedes the
// pending navigation: its dialog closes and the tab close confirmation is shown
// instead. Cancelling keeps the tab, the original page and the task, and the
// superseded navigation is not resumed.
IN_PROC_BROWSER_TEST_F(ActorUiTabControllerNavigationConfirmTest,
                       UserUiNavigation_CloseTabWhileDialogOpen_Cancel) {
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  const GURL destination_url = embedded_test_server()->GetURL("/title2.html");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  tabs::TabInterface* tab = tab_strip->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), initial_url));
  // Keep a second tab so closing the acting tab does not close the browser.
  ASSERT_TRUE(AddTabAtIndex(1, initial_url,
                            ::ui::PageTransition::PAGE_TRANSITION_TYPED));
  tab_strip->ActivateTabAt(tab_strip->GetIndexOfTab(tab));

  ActorUiTabController* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  actor::TaskId task_id = StartTaskOnTabs({tab}, /*stop_task_on_detach=*/true);

  NavigateViaUserUi(tab, destination_url, OmniboxTransition());
  views::Widget* navigation_dialog = WaitForConfirmDialog(controller);
  ASSERT_NE(navigation_dialog, nullptr);

  views::test::WidgetDestroyedWaiter navigation_dialog_destroyed(
      navigation_dialog);
  tab_strip->CloseWebContentsAt(tab_strip->GetIndexOfTab(tab),
                                TabCloseTypes::CLOSE_USER_GESTURE);
  navigation_dialog_destroyed.Wait();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetTabCloseConfirmDialog() != nullptr; }));

  // The tab stays open until the close confirmation is answered.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_NE(tab_strip->GetIndexOfTab(tab), TabStripModel::kNoTab);

  GetTabCloseConfirmDialog()
      ->widget_delegate()
      ->AsDialogDelegate()
      ->CancelDialog();

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_FALSE(tab->GetContents()->IsLoading());
  EXPECT_EQ(tab->GetContents()->GetController().GetPendingEntry(), nullptr);
  EXPECT_EQ(tab->GetContents()->GetLastCommittedURL(), initial_url);
  EXPECT_EQ(controller->GetActiveNavigationConfirmDialogWidgetForTesting(),
            nullptr);
  EXPECT_NE(actor_keyed_service()->GetTask(task_id), nullptr);

  actor_keyed_service()->StopTask(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
}

// As above, but accepting the tab close confirmation closes the tab and stops
// the task as a user stop rather than as a navigation away.
IN_PROC_BROWSER_TEST_F(ActorUiTabControllerNavigationConfirmTest,
                       UserUiNavigation_CloseTabWhileDialogOpen_Accept) {
  base::HistogramTester histogram_tester;
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  const GURL destination_url = embedded_test_server()->GetURL("/title2.html");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  tabs::TabInterface* tab = tab_strip->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), initial_url));
  // Keep a second tab so closing the acting tab does not close the browser.
  ASSERT_TRUE(AddTabAtIndex(1, initial_url,
                            ::ui::PageTransition::PAGE_TRANSITION_TYPED));
  tab_strip->ActivateTabAt(tab_strip->GetIndexOfTab(tab));

  ActorUiTabController* controller = ActorUiTabController::From(tab);
  ASSERT_NE(controller, nullptr);

  actor::TaskId task_id = StartTaskOnTabs({tab}, /*stop_task_on_detach=*/true);

  NavigateViaUserUi(tab, destination_url, OmniboxTransition());
  views::Widget* navigation_dialog = WaitForConfirmDialog(controller);
  ASSERT_NE(navigation_dialog, nullptr);

  views::test::WidgetDestroyedWaiter navigation_dialog_destroyed(
      navigation_dialog);
  tab_strip->CloseWebContentsAt(tab_strip->GetIndexOfTab(tab),
                                TabCloseTypes::CLOSE_USER_GESTURE);
  navigation_dialog_destroyed.Wait();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetTabCloseConfirmDialog() != nullptr; }));

  // The tab stays open until the close confirmation is answered.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_NE(actor_keyed_service()->GetTask(task_id), nullptr);

  content::WebContentsDestroyedWatcher tab_destroyed(tab->GetContents());
  GetTabCloseConfirmDialog()
      ->widget_delegate()
      ->AsDialogDelegate()
      ->AcceptDialog();
  tab_destroyed.Wait();

  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id), nullptr);
  histogram_tester.ExpectUniqueSample(
      "Actor.Task.StoppedReason",
      actor::ActorTask::StoppedReason::kStoppedByUser, 1);
}

}  // namespace
}  // namespace actor::ui
