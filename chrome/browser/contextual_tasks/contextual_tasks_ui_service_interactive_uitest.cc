// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
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
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

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
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      const base::Uuid& task_id)
      : contextual_tasks_service_(contextual_tasks_service),
        task_id_(task_id) {}

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents : change.GetInsert()->contents) {
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(contents.contents);
        std::optional<ContextualTask> task =
            contextual_tasks_service_->GetContextualTaskForTab(session_id);
        if (task.has_value()) {
          EXPECT_EQ(task->GetTaskId(), task_id_);
          was_inserted_ = true;
        }
      }
    }
  }

  bool was_inserted() const { return was_inserted_; }

 private:
  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;
  base::Uuid task_id_;
  bool was_inserted_ = false;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnThreadLinkClicked_CreatesNewTabAndAssociates) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Add a new tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create task1 and associate with tab0.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(
          browser()->tab_strip_model()->GetWebContentsAt(0)));

  TabStripModelObserverImpl observer(contextual_tasks_service,
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
            coordinator->GetActiveWebContents();
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
  histogram_tester.ExpectUniqueSample("ContextualTasks.ActiveTasksCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnThreadLinkClicked_CreatesNewTabInSameGroup) {
  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());

  // Add a contextual-tasks tab and add it to a group.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  chrome::AddTabAt(browser(),
                   GURL("chrome://contextual-tasks/?task=" +
                        task1.GetTaskId().AsLowercaseString()),
                   -1, false);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* task_tab = tab_strip_model->GetActiveTab();
  browser()->GetTabStripModel()->AddToNewGroup(
      {tab_strip_model->GetIndexOfTab(task_tab)});
  content::WaitForLoadStop(task_tab->GetContents());

  ASSERT_TRUE(task_tab->GetGroup().has_value());
  tab_groups::TabGroupId group_id = task_tab->GetGroup().value();

  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  RunTestSequence(Do([&]() {
                    // Fake a link click interception.
                    const GURL clicked_url("https://google.com/");
                    service->OnThreadLinkClicked(clicked_url, task1.GetTaskId(),
                                                 task_tab->GetWeakPtr(),
                                                 browser()->GetWeakPtr());
                  }),
                  WaitForShow(kContextualTasksSidePanelWebViewElementId));
  ASSERT_EQ(tab_strip_model->GetActiveTab()->GetGroup(), group_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnThreadLinkClicked_CanNavigateBack) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());

  // Add a contextual-tasks tab and add it to a group.
  ContextualTask task1 = contextual_tasks_service->CreateTask();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* task_tab = tab_strip_model->GetActiveTab();

  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Fake a link click interception.
  service->OnThreadLinkClicked(GURL(chrome::kChromeUIHistoryURL),
                               task1.GetTaskId(), task_tab->GetWeakPtr(),
                               browser()->GetWeakPtr());

  // Wait for the navigation to finish.
  {
    content::TestNavigationObserver observer(
        browser()->GetActiveTabInterface()->GetContents());
    observer.WaitForNavigationFinished();
  }

  // Verify the side panel is open.
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());

  // Verify the new tab can navigation back.
  EXPECT_TRUE(browser()
                  ->GetActiveTabInterface()
                  ->GetContents()
                  ->GetController()
                  .CanGoBack());

  // Trigger the back button.
  browser()->GetActiveTabInterface()->GetContents()->GetController().GoBack();

  // Wait for the navigation to finish.
  {
    content::TestNavigationObserver observer(
        browser()->GetActiveTabInterface()->GetContents());
    observer.WaitForNavigationFinished();
  }

  // Verify the side panel is closed.
  EXPECT_FALSE(coordinator->IsSidePanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnThreadLinkClicked_ActivateExistingTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Add two more tabs so there are three total.
  const GURL version_url(chrome::kChromeUIVersionURL);
  chrome::AddTabAt(browser(), version_url, -1, false);
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(1));

  const GURL settings_url(chrome::kChromeUISettingsURL);
  chrome::AddTabAt(browser(), settings_url, -1, true);
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(2));

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Associate the tabs with the task.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                             tab_strip_model->GetWebContentsAt(0)));
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                             tab_strip_model->GetWebContentsAt(1)));
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                             tab_strip_model->GetWebContentsAt(2)));

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // There should only be three tabs.
        ASSERT_EQ(3, tab_strip_model->count());

        // The selected tab should be settings.
        ASSERT_EQ(
            settings_url,
            tab_strip_model->GetActiveWebContents()->GetLastCommittedURL());

        // Simulate a link click to a URL that's already open in a
        // tab.
        service->OnThreadLinkClicked(version_url, task1.GetTaskId(), nullptr,
                                     browser()->GetWeakPtr());

        // There should still only be three tabs.
        ASSERT_EQ(3, tab_strip_model->count());

        // The selected tab should have switched back to the version page.
        ASSERT_EQ(
            version_url,
            tab_strip_model->GetActiveWebContents()->GetLastCommittedURL());
      }));
}

// Ensure that an existing tab is not focused if it isn't affiliated with the
// task.
IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnThreadLinkClicked_ActivateExistingTab_NoAffiliation) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Add two more tabs so there are three total.
  const GURL version_url(chrome::kChromeUIVersionURL);
  chrome::AddTabAt(browser(), version_url, -1, false);
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(1));

  const GURL settings_url(chrome::kChromeUISettingsURL);
  chrome::AddTabAt(browser(), settings_url, -1, true);
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(2));

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Associate all but the version page with the task.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                             tab_strip_model->GetWebContentsAt(0)));
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                             tab_strip_model->GetWebContentsAt(2)));

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // There should only be three tabs.
        ASSERT_EQ(3, tab_strip_model->count());

        // The selected tab should be settings.
        ASSERT_EQ(
            settings_url,
            tab_strip_model->GetActiveWebContents()->GetLastCommittedURL());

        // Simulate a link click to a URL that's already open in a
        // tab.
        service->OnThreadLinkClicked(version_url, task1.GetTaskId(), nullptr,
                                     browser()->GetWeakPtr());

        // Another tab should have been added
        ASSERT_EQ(4, tab_strip_model->count());

        // Wait for the new tab to finish loading.
        content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(3));

        // The selected tab should now be the new version page.
        ASSERT_EQ(
            version_url,
            tab_strip_model->GetActiveWebContents()->GetLastCommittedURL());
      }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksUiServiceInteractiveUiTest,
    OnTaskChanged_SwitchAllTabAffiliation_ActivatesMostRecentTab) {
  // Add two new tabs.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, true);
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIHistoryURL), -1, true);

  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create two tasks.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  ContextualTask task2 = contextual_tasks_service->CreateTask();

  // Associate the two new tabs with the first task.
  content::WebContents* tab1_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* tab2_contents =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  SessionID tab1_id = sessions::SessionTabHelper::IdForTab(tab1_contents);
  SessionID tab2_id = sessions::SessionTabHelper::IdForTab(tab2_contents);
  contextual_tasks_service->AssociateTabWithTask(task1.GetTaskId(), tab1_id);
  contextual_tasks_service->AssociateTabWithTask(task1.GetTaskId(), tab2_id);

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Call OnTaskChanged and verify that both tabs are now associated with
  // the second task.
  service->OnTaskChanged(browser(), nullptr, task2.GetTaskId(), false);
  EXPECT_EQ(
      task2.GetTaskId(),
      contextual_tasks_service->GetContextualTaskForTab(tab1_id)->GetTaskId());
  EXPECT_EQ(
      task2.GetTaskId(),
      contextual_tasks_service->GetContextualTaskForTab(tab2_id)->GetTaskId());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnTaskChanged_WithInvalidTaskId) {
  // Add two new tabs.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, true);
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIHistoryURL), -1, true);

  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create two tasks.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  ContextualTask task2 = contextual_tasks_service->CreateTask();

  // Associate the two new tabs with the 2 tasks.
  content::WebContents* tab1_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* tab2_contents =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  SessionID tab1_id = sessions::SessionTabHelper::IdForTab(tab1_contents);
  SessionID tab2_id = sessions::SessionTabHelper::IdForTab(tab2_contents);
  contextual_tasks_service->AssociateTabWithTask(task1.GetTaskId(), tab1_id);
  contextual_tasks_service->AssociateTabWithTask(task2.GetTaskId(), tab2_id);

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Call OnTaskChanged and verify that the first tab is now associated
  // with an empty task.
  service->OnTaskChanged(browser(), nullptr, base::Uuid(), false);
  std::optional<ContextualTask> empty_task =
      contextual_tasks_service->GetContextualTaskForTab(tab1_id);

  EXPECT_NE(task2.GetTaskId(), empty_task->GetTaskId());
  EXPECT_NE(task1.GetTaskId(), empty_task->GetTaskId());
  EXPECT_FALSE(empty_task->GetThread());
  EXPECT_EQ(empty_task->GetTabIds().size(), 1u);
  EXPECT_EQ(empty_task->GetTabIds()[0], tab1_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnTaskChanged_ShownInTab_DoesNotSwitchTabAffiliation) {
  // Add two new tabs.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, true);
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIHistoryURL), -1, true);

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create two tasks.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  ContextualTask task2 = contextual_tasks_service->CreateTask();

  // Associate the two new tabs with the first task.
  content::WebContents* tab1_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* tab2_contents =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  SessionID tab1_id = sessions::SessionTabHelper::IdForTab(tab1_contents);
  SessionID tab2_id = sessions::SessionTabHelper::IdForTab(tab2_contents);
  contextual_tasks_service->AssociateTabWithTask(task1.GetTaskId(), tab1_id);
  contextual_tasks_service->AssociateTabWithTask(task1.GetTaskId(), tab2_id);

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Call OnTaskChanged with is_shown_in_tab = true and verify that tabs
  // remain associated with the first task.
  service->OnTaskChanged(browser(), nullptr, task2.GetTaskId(), true);
  EXPECT_EQ(
      task1.GetTaskId(),
      contextual_tasks_service->GetContextualTaskForTab(tab1_id)->GetTaskId());
  EXPECT_EQ(
      task1.GetTaskId(),
      contextual_tasks_service->GetContextualTaskForTab(tab2_id)->GetTaskId());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       StartTaskUiInSidePanel) {
  // Add a new tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  const GURL search_url("https://google.com/search");

  // Call StartTaskUiInSidePanel and verify that the side panel is shown and the
  // task is associated with the active tab.
  RunTestSequence(
      Do([&]() {
        service->StartTaskUiInSidePanel(
            browser(), browser()->GetActiveTabInterface(), search_url, nullptr);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        ContextualTasksSidePanelCoordinator* coordinator =
            ContextualTasksSidePanelCoordinator::From(browser());
        EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());

        SessionID tab_id = sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetActiveWebContents());
        std::optional<ContextualTask> task =
            contextual_tasks_service->GetContextualTaskForTab(tab_id);
        EXPECT_TRUE(task.has_value());
        EXPECT_EQ(service->GetInitialUrlForTask(task->GetTaskId()), search_url);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       StartTaskUiInSidePanel_WhenSidePanelIsOpen) {
  // Add a new tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  const GURL search_url("https://google.com/search");
  RunTestSequence(
      Do([&]() {
        service->StartTaskUiInSidePanel(
            browser(), browser()->GetActiveTabInterface(), search_url, nullptr);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        ContextualTasksSidePanelCoordinator* coordinator =
            ContextualTasksSidePanelCoordinator::From(browser());
        EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());

        SessionID tab_id = sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetActiveWebContents());
        std::optional<ContextualTask> task =
            contextual_tasks_service->GetContextualTaskForTab(tab_id);
        EXPECT_TRUE(task.has_value());
        base::Uuid initial_task_id = task->GetTaskId();

        // Call StartTaskUiInSidePanel again.
        const GURL search_url2("https://google.com/search?q=foo");
        service->StartTaskUiInSidePanel(browser(),
                                        browser()->GetActiveTabInterface(),
                                        search_url2, nullptr);

        // Verify that the task ID is still the same.
        std::optional<ContextualTask> task2 =
            contextual_tasks_service->GetContextualTaskForTab(tab_id);
        EXPECT_TRUE(task2.has_value());
        EXPECT_EQ(task2->GetTaskId(), initial_task_id);
        EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());
      }));
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
            coordinator->GetActiveWebContents();
        ContextualTasksUI* ui = static_cast<ContextualTasksUI*>(
            web_contents->GetWebUI()->GetController());

        ASSERT_TRUE(ui);
        ui->DisableActiveTabContextSuggestion();
        EXPECT_FALSE(ui_service->auto_tab_context_suggestion_enabled());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       MoveTaskUiToNewTab) {
  // Add 2 new tabs.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Create task1 and associate with tab0.
  ContextualTask task1 = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(
          browser()->tab_strip_model()->GetWebContentsAt(0)));
  contextual_tasks_service->AssociateTabWithTask(
      task1.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(
          browser()->tab_strip_model()->GetWebContentsAt(1)));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // 3 tabs open.
  EXPECT_EQ(tab_strip_model->count(), 3);

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());
        tab_strip_model->ActivateTabAt(1);

        // The side panel will remain open because the tasks are assocaiated
        // with the same task.
        EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());
        EXPECT_EQ(tab_strip_model->count(), 3);
        EXPECT_EQ(tab_strip_model->active_index(), 1);

        // Moving the task UI to a new tab will disassocaite all tabs from this
        // task.
        service->MoveTaskUiToNewTab(task1.GetTaskId(), browser(),
                                    GURL(chrome::kChromeUIContextualTasksURL));
      }),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        EXPECT_EQ(tab_strip_model->count(), 4);
        EXPECT_EQ(tab_strip_model->active_index(), 2);
        EXPECT_FALSE(coordinator->IsSidePanelOpenForContextualTask());

        // Go back to original tab and open the  side panel
        // again.
        tab_strip_model->ActivateTabAt(0);
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());

        tab_strip_model->ActivateTabAt(1);
      }),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // The side panel will hide because the 2 tabs are no longer associated
        // with the same task.
        EXPECT_FALSE(coordinator->IsSidePanelOpenForContextualTask());
      }));
}

class ContextualTasksUiServiceWithoutSidePanelInteractiveUiTest
    : public ContextualTasksUiServiceInteractiveUiTest {
 public:
  ContextualTasksUiServiceWithoutSidePanelInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kContextualTasks, {{"OpenSidePanelOnLinkClicked", "false"}});
  }
  ~ContextualTasksUiServiceWithoutSidePanelInteractiveUiTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksUiServiceWithoutSidePanelInteractiveUiTest,
    OnThreadLinkClicked_DoNotOpenSidePanel) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());

  // Add a contextual-tasks tab and add it to a group.
  ContextualTask task1 = contextual_tasks_service->CreateTask();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* task_tab = tab_strip_model->GetActiveTab();

  ContextualTasksUiService* service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(service);

  // Fake a link click interception.
  service->OnThreadLinkClicked(GURL(chrome::kChromeUIHistoryURL),
                               task1.GetTaskId(), task_tab->GetWeakPtr(),
                               browser()->GetWeakPtr());

  // Wait for the navigation to finish.
  content::TestNavigationObserver observer(
      browser()->GetActiveTabInterface()->GetContents());
  observer.WaitForNavigationFinished();

  // Verify the side panel is not open.
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  EXPECT_FALSE(coordinator->IsSidePanelOpenForContextualTask());
}

}  // namespace contextual_tasks
