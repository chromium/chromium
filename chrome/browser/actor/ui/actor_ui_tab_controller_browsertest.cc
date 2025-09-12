// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

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

class ActorUiTabControllerTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiTabIndicator.name, "true"}});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
      tab_alert_controller->IsAlertActive(tabs::TabAlert::ACTOR_ACCESSING));

  // Start acting on the tab.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  ASSERT_TRUE(result.Wait());
  actor::ExpectOkResult(result);

  // The indicator should now be visible.
  EXPECT_TRUE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::ACTOR_ACCESSING));

  // Stop acting on the tab.
  state_manager->OnUiEvent(StoppedActingOnTab(tab->GetHandle()));

  // The indicator should be hidden again.
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::ACTOR_ACCESSING));
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
      tab_alert_controller->IsAlertActive(tabs::TabAlert::ACTOR_ACCESSING));

  // Start acting on the tab.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  actor::ExpectOkResult(result);

  // The indicator should still not be visible.
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::ACTOR_ACCESSING));
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

class ActorUiTabControllerDisabledTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiTabIndicator.name, "false"}});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
      tab_alert_controller->IsAlertActive(tabs::TabAlert::ACTOR_ACCESSING));

  // Start acting on the tab.
  TestFuture<ActionResultPtr> result;
  state_manager->OnUiEvent(
      StartingToActOnTab(tab->GetHandle(), actor::TaskId(1)),
      result.GetCallback());
  actor::ExpectOkResult(result);

  // The indicator should still not be visible.
  EXPECT_FALSE(
      tab_alert_controller->IsAlertActive(tabs::TabAlert::ACTOR_ACCESSING));
}

}  // namespace
}  // namespace actor::ui
