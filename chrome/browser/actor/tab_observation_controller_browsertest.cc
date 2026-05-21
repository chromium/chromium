// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_observation_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace actor {

class TabObservationControllerBrowserTest : public InProcessBrowserTest {
 public:
  TabObservationControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kGlicActor}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  ActorKeyedService* actor_service() {
    return ActorKeyedService::Get(browser()->profile());
  }

  TaskId CreateTask() {
    return actor_service()->CreateTask(TestTaskSourceInfo(),
                                       NoEnterprisePolicyChecker());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest, ObserveSingleTab) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->profile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {}, future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].id(), tab->GetHandle().raw_value());
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveMultipleTabs) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  tabs::TabInterface* tab1 = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab1, *actor_service()->GetTask(task_id));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tabs::TabInterface* tab2 = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab2, *actor_service()->GetTask(task_id));

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->profile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {}, future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  EXPECT_EQ(result->tab_observations.size(), 2u);

  bool found_tab1 = false;
  bool found_tab2 = false;
  for (const auto& obs : result->tab_observations) {
    if (obs.id() == tab1->GetHandle().raw_value()) {
      found_tab1 = true;
    } else if (obs.id() == tab2->GetHandle().raw_value()) {
      found_tab2 = true;
    }
    EXPECT_EQ(obs.result(),
              optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  }
  EXPECT_TRUE(found_tab1);
  EXPECT_TRUE(found_tab2);
}

IN_PROC_BROWSER_TEST_F(TabObservationControllerBrowserTest,
                       ObserveCrashedTabWithReload) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  // Crash the tab.
  content::RenderProcessHost* process =
      tab->GetContents()->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(content::RESULT_CODE_KILLED);
  crash_observer.Wait();
  ASSERT_TRUE(tab->GetContents()->IsCrashed());

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->profile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {}, future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].id(), tab->GetHandle().raw_value());
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_FALSE(tab->GetContents()->IsCrashed());
}

class TabObservationControllerWithoutScreenshotBrowserTest
    : public TabObservationControllerBrowserTest {
 public:
  TabObservationControllerWithoutScreenshotBrowserTest() {
    screenshot_feature_list_.InitAndEnableFeature(kGlicActorSkipScreenshot);
  }

 private:
  base::test::ScopedFeatureList screenshot_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabObservationControllerWithoutScreenshotBrowserTest,
                       ObserveWithoutScreenshot) {
  TaskId task_id = CreateTask();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  actor::AddTabToTask(*tab, *actor_service()->GetTask(task_id));

  base::test::TestFuture<TabObservationController*,
                         std::unique_ptr<ObservationResult>>
      future;
  TabObservationController controller(
      browser()->profile(), task_id, base::TimeTicks::Now(),
      /*skip_async_observation_information=*/false, {}, future.GetCallback());
  controller.Start();

  auto [controller_ptr, result] = future.Take();
  ASSERT_EQ(result->tab_observations.size(), 1u);
  EXPECT_EQ(result->tab_observations[0].result(),
            optimization_guide::proto::TabObservation::TAB_OBSERVATION_OK);
  EXPECT_EQ(result->tab_observations[0].screenshot_result(),
            optimization_guide::proto::TabObservation::SCREENSHOT_OK);
  EXPECT_FALSE(result->tab_observations[0].has_screenshot());
}

}  // namespace actor
