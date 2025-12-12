// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using testing::_;
using testing::SaveArg;

namespace contextual_tasks {

class ContextualTasksUiServiceInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  ContextualTasksUiServiceInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(kContextualTasks);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TabStripModelObserverImpl : public TabStripModelObserver {
 public:
  explicit TabStripModelObserverImpl(
      ContextualTasksContextController* controller,
      const base::Uuid& task_id)
      : controller_(controller), task_id_(task_id) {}

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents : change.GetInsert()->contents) {
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(contents.contents);
        std::optional<ContextualTask> task =
            controller_->GetContextualTaskForTab(session_id);
        if (task.has_value()) {
          EXPECT_EQ(task->GetTaskId(), task_id_);
          was_inserted_ = true;
        }
      }
    }
  }

  bool was_inserted() const { return was_inserted_; }

 private:
  raw_ptr<ContextualTasksContextController> controller_;
  base::Uuid task_id_;
  bool was_inserted_ = false;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnThreadLinkClicked_CreatesNewTabAndAssociates) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Add a new tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create task1 and associate with tab0.
  ContextualTask task1 = contextual_tasks_controller->CreateTask();
  contextual_tasks_controller->AssociateTabWithTask(
      task1.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(
          browser()->tab_strip_model()->GetWebContentsAt(0)));

  TabStripModelObserverImpl observer(contextual_tasks_controller,
                                     task1.GetTaskId());
  browser()->tab_strip_model()->AddObserver(&observer);

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        const GURL source_url("chrome://contextual-tasks/?task=" +
                              task1.GetTaskId().AsLowercaseString());
        content::NavigationController::LoadURLParams load_params(source_url);
        content::WebContents* panel_contents =
            coordinator->GetActiveWebContentsForTesting();
        panel_contents->GetController().LoadURLWithParams(load_params);
        content::WaitForLoadStop(panel_contents);

        // Call the OnThreadLinkClicked() method.
        const GURL clicked_url("https://google.com/");
        service->OnThreadLinkClicked(clicked_url, task1.GetTaskId(), nullptr,
                                     browser()->GetWeakPtr());

        EXPECT_TRUE(observer.was_inserted());
      }));
  browser()->tab_strip_model()->RemoveObserver(&observer);

  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.AiResponse.UserAction.LinkClicked.Panel", true, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.AiResponse.UserAction.LinkClicked.Panel"),
            1);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksUiServiceInteractiveUiTest,
    OnTaskChangedInPanel_SwitchAllTabAffiliation_ActivatesMostRecentTab) {
  // Add two new tabs.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, true);
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIHistoryURL), -1, true);

  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create two tasks.
  ContextualTask task1 = contextual_tasks_controller->CreateTask();
  ContextualTask task2 = contextual_tasks_controller->CreateTask();

  // Associate the two new tabs with the first task.
  content::WebContents* tab1_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* tab2_contents =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  SessionID tab1_id = sessions::SessionTabHelper::IdForTab(tab1_contents);
  SessionID tab2_id = sessions::SessionTabHelper::IdForTab(tab2_contents);
  contextual_tasks_controller->AssociateTabWithTask(task1.GetTaskId(), tab1_id);
  contextual_tasks_controller->AssociateTabWithTask(task1.GetTaskId(), tab2_id);

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Call OnTaskChangedInPanel and verify that both tabs are now associated with
  // the second task.
  service->OnTaskChangedInPanel(browser(), nullptr, task2.GetTaskId());
  EXPECT_EQ(task2.GetTaskId(),
            contextual_tasks_controller->GetContextualTaskForTab(tab1_id)
                ->GetTaskId());
  EXPECT_EQ(task2.GetTaskId(),
            contextual_tasks_controller->GetContextualTaskForTab(tab2_id)
                ->GetTaskId());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnTaskChangedInPanel_WithInvalidTaskId) {
  // Add two new tabs.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, true);
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIHistoryURL), -1, true);

  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create two tasks.
  ContextualTask task1 = contextual_tasks_controller->CreateTask();
  ContextualTask task2 = contextual_tasks_controller->CreateTask();

  // Associate the two new tabs with the 2 tasks.
  content::WebContents* tab1_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* tab2_contents =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  SessionID tab1_id = sessions::SessionTabHelper::IdForTab(tab1_contents);
  SessionID tab2_id = sessions::SessionTabHelper::IdForTab(tab2_contents);
  contextual_tasks_controller->AssociateTabWithTask(task1.GetTaskId(), tab1_id);
  contextual_tasks_controller->AssociateTabWithTask(task2.GetTaskId(), tab2_id);

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Call OnTaskChangedInPanel and verify that the first tab is now associated
  // with an empty task.
  service->OnTaskChangedInPanel(browser(), nullptr, base::Uuid());
  std::optional<ContextualTask> empty_task =
      contextual_tasks_controller->GetContextualTaskForTab(tab1_id);

  EXPECT_NE(task2.GetTaskId(), empty_task->GetTaskId());
  EXPECT_NE(task1.GetTaskId(), empty_task->GetTaskId());
  EXPECT_FALSE(empty_task->GetThread());
  EXPECT_EQ(empty_task->GetTabIds().size(), 1u);
  EXPECT_EQ(empty_task->GetTabIds()[0], tab1_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       StartTaskUiInSidePanel) {
  // Add a new tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Call StartTaskUiInSidePanel and verify that the side panel is shown and the
  // task is associated with the active tab.
  const GURL search_url("https://google.com/search");
  service->StartTaskUiInSidePanel(browser(), browser()->GetActiveTabInterface(),
                                  search_url);

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());

  SessionID tab_id = sessions::SessionTabHelper::IdForTab(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::optional<ContextualTask> task =
      contextual_tasks_controller->GetContextualTaskForTab(tab_id);
  EXPECT_TRUE(task.has_value());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       DisableTabSuggestionAfterRemoving) {
  // Add a new tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(ui_service);
  EXPECT_TRUE(ui_service->auto_tab_context_suggestion_enabled());

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        content::WebContents* web_contents =
            coordinator->GetActiveWebContentsForTesting();
        ContextualTasksUI* ui = static_cast<ContextualTasksUI*>(
            web_contents->GetWebUI()->GetController());

        ASSERT_TRUE(ui);
        ui->DisableActiveTabContextSuggestion();
        EXPECT_FALSE(ui_service->auto_tab_context_suggestion_enabled());
      }));
}

}  // namespace contextual_tasks
