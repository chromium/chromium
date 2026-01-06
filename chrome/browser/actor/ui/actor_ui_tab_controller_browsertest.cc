// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/animated_image_view.h"
#include "url/gurl.h"

namespace actor::ui {
namespace {

using actor::mojom::ActionResultPtr;
using base::test::TestFuture;

class FutureTabStripModelObserver : public TabStripModelObserver {
 public:
  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    if (change_type == TabChangeType::kAll) {
      Reset();
      future_.SetValue();
    }
  }

  // Returns true if the future was fulfilled.
  bool Wait() { return future_.Wait(); }

  // Resets the future for the next event.
  void Reset() { future_.Clear(); }

 private:
  base::test::TestFuture<void> future_;
};

class BaseActorUiTabControllerTest : public InProcessBrowserTest {
 protected:
  views::AnimatedImageView* GetSpinner() {
    TabStripViewInterface* tab_strip_view =
        browser()->window()->AsBrowserView()->tab_strip_view();
    Tab* tab_specific = tab_strip_view->GetTabAnchorViewAt(
        browser()->tab_strip_model()->active_index());
    views::AnimatedImageView* spinner =
        views::AsViewClass<AlertIndicatorButton>(
            tab_specific->GetViewByElementId(kTabAlertIndicatorButtonElementId))
            ->GetActorIndicatorSpinnerForTesting();
    return spinner;
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    actor_keyed_service()->GetPolicyChecker().SetActOnWebForTesting(true);
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->profile());
  }

  base::test::ScopedFeatureList feature_list_;
};

class ActorUiTabControllerTest : public BaseActorUiTabControllerTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicActorUi,
          {{features::kGlicActorUiTabIndicator.name, "true"}}},
         {features::kGlicActorUiTabIndicatorSpinnerIgnoreReducedMotion, {}}},
        {});
    InProcessBrowserTest::SetUp();
  }
};

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(ActorUiTabControllerTest,
                       TabIndicatorVisibleDuringActuation) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
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
  actor_service->GetPolicyChecker().SetActOnWebForTesting(true);
  actor::TaskId task_id = actor_service->CreateTask();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  actor::ui::StartTask start_task_event(task_id);
  actor_service->GetActorUiStateManager()->OnUiEvent(start_task_event);
  // Need to wait for the AUSM to notify the GlicActorTaskIconManager.
  base::PlatformThread::Sleep(actor::ui::kProfileScopedUiUpdateDebounceDelay);

  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0,
                                     GURL(chrome::kChromeUINewTabURL),
                                     ::ui::PAGE_TRANSITION_LINK));
  auto* tab_one = browser()->GetTabStripModel()->GetTabAtIndex(0);
  base::RunLoop loop;
  task->AddTab(
      tab_one->GetHandle(),
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
  actor_service->GetActorUiStateManager()->OnUiEvent(
      actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kWaitingOnUser, /*title=*/""));
  // Need to wait for the AUSM to notify the GlicActorTaskIconManager.
  base::PlatformThread::Sleep(actor::ui::kProfileScopedUiUpdateDebounceDelay);

  // The static icon should be visible, but not the spinner.
  EXPECT_TRUE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorWaitingOnUser));
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::kActorAccessing));
  EXPECT_EQ(GetSpinner()->state(), views::AnimatedImageView::State::kStopped);

  // Restart the task
  actor_service->GetActorUiStateManager()->OnUiEvent(
      actor::ui::TaskStateChanged(task_id, actor::ActorTask::State::kActing,
                                  /*title=*/""));
  // Need to wait for the AUSM to notify the GlicActorTaskIconManager.
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
  TaskId task_id = actor_keyed_service()->CreateTask();

  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank?1"),
                            ::ui::PageTransition::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* actuating_tab =
      browser()->tab_strip_model()->GetActiveTab();

  // Start acting on the tab.
  base::RunLoop loop;
  actor_keyed_service()->GetTask(task_id)->AddTab(
      actuating_tab->GetHandle(),
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
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfTab(actuating_tab));

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
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfTab(actuating_tab));

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.ActuatingTabWebContentsAttached"));
}

#else   // !BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(ActorUiTabControllerTest,
                       TabIndicatorNotVisibleWhenGlicIsDisabled) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
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
#endif  // BUILDFLAG(ENABLE_GLIC)

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(ActorUiTabControllerTest,
                       TabStripModelNotifiedOnUpdate) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(tab, nullptr);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  FutureTabStripModelObserver observer;
  tab_strip_model->AddObserver(&observer);

  // The observer should be notified when the indicator is shown.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  actor::ExpectOkResult(result);

  EXPECT_TRUE(observer.Wait());

  // The observer should also be notified when the indicator is hidden.
  observer.Reset();
  state_manager->OnUiEvent(StoppedActingOnTab(tab->GetHandle()));
  EXPECT_TRUE(observer.Wait());

  tab_strip_model->RemoveObserver(&observer);
}
#endif  // BUILDFLAG(ENABLE_GLIC)

class ActorUiTabControllerDisabledTest : public BaseActorUiTabControllerTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiTabIndicator.name, "false"}});
    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(ActorUiTabControllerDisabledTest,
                       TabIndicatorNotVisibleWhenFeatureDisabled) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
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
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicActorUi,
          {{features::kGlicActorUiTabIndicator.name, "true"}}}},
        {features::kGlicActorUiTabIndicatorSpinnerIgnoreReducedMotion});
    InProcessBrowserTest::SetUp();
  }
};

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(ActorUiTabIndicatorSpinnerIgnoreReducedMotionDisabled,
                       TabIndicatorVisibleDuringActuation) {
  Profile* const profile = browser()->profile();
  ActorUiStateManagerInterface* state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  ASSERT_NE(state_manager, nullptr);
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
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
#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace
}  // namespace actor::ui
