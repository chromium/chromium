// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_contents_tester.h"

using testing::AtLeast;
using testing::Field;
using testing::IsEmpty;
using testing::Mock;
using testing::Not;
using testing::Pointee;

namespace contextual_tasks {

class MockContextualTasksComposeboxHandler
    : public ContextualTasksComposeboxHandler {
 public:
  MockContextualTasksComposeboxHandler(
      ContextualTasksUI* ui_controller,
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler)
      : ContextualTasksComposeboxHandler(ui_controller,
                                         profile,
                                         web_contents,
                                         std::move(pending_handler),
                                         std::move(pending_page),
                                         std::move(pending_searchbox_handler)) {
  }
  ~MockContextualTasksComposeboxHandler() override = default;

  MOCK_METHOD(void,
              UpdateSuggestedTabContext,
              (searchbox::mojom::TabInfoPtr tab_info),
              (override));
};

class MockActiveTaskContextProviderObserver
    : public ActiveTaskContextProvider::Observer {
 public:
  MockActiveTaskContextProviderObserver() = default;
  ~MockActiveTaskContextProviderObserver() override = default;

  MOCK_METHOD(void, OnContextTabsChanged, (const std::set<tabs::TabHandle>&));
};

class ContextualTasksSidePanelCoordinatorInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  ContextualTasksSidePanelCoordinatorInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(kContextualTasks);
  }
  ~ContextualTasksSidePanelCoordinatorInteractiveUiTest() override = default;

  void SetUpTasks() {
    browser()
        ->GetFeatures()
        .contextual_tasks_active_task_context_provider()
        ->AddObserver(&mock_active_task_context_provider_observer_);

    // Add tab1.
    chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
    // Add tab2.
    chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
    // Add tab3.
    chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

    ContextualTasksContextController* contextual_tasks_controller =
        ContextualTasksContextControllerFactory::GetForProfile(
            browser()->profile());

    // Create task1 and associate with tab0 and tab2, create task2 and associate
    // with tab1. Left tab3 with no task associated with.
    ContextualTask task1 = contextual_tasks_controller->CreateTask();
    task_id1_ = task1.GetTaskId();
    contextual_tasks_controller->AssociateTabWithTask(
        task1.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(0)));
    ContextualTask task2 = contextual_tasks_controller->CreateTask();
    task_id2_ = task2.GetTaskId();
    contextual_tasks_controller->AssociateTabWithTask(
        task2.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(1)));
    contextual_tasks_controller->AssociateTabWithTask(
        task1.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(2)));

    // CachedWebContents are only created when transferring a tab to the side
    // panel or when calling Show(). Use the test-only method to imitate a
    // session where the side panel has been created for each of these tasks.
    ContextualTasksSidePanelCoordinator* coordinator =
        ContextualTasksSidePanelCoordinator::From(browser());
    coordinator->CreateCachedWebContentsForTesting(task_id1_, /*is_open=*/true);
    coordinator->CreateCachedWebContentsForTesting(task_id2_, /*is_open=*/true);

    browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  ContextualTasksUI* GetContextualTasksUI() {
    ContextualTasksSidePanelCoordinator* coordinator =
        ContextualTasksSidePanelCoordinator::From(browser());
    content::WebContents* web_contents = coordinator->GetActiveWebContents();
    if (!web_contents) {
      return nullptr;
    }
    ContextualTasksUI* ui = static_cast<ContextualTasksUI*>(
        web_contents->GetWebUI()->GetController());
    return ui;
  }

 protected:
  base::Uuid task_id1_;
  base::Uuid task_id2_;
  MockActiveTaskContextProviderObserver
      mock_active_task_context_provider_observer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

MATCHER(IsNullSuggestedTabContext, "is a null TabContextPtr") {
  return arg.is_null();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SwitchTabChangeSidePanelWebContents) {
  SetUpTasks();
  EXPECT_CALL(mock_active_task_context_provider_observer_,
              OnContextTabsChanged(testing::_))
      .Times(AtLeast(1));
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify the first side panel WebContents is created for the first tab.
        content::WebContents* side_panel_web_contents1 =
            coordinator->GetActiveWebContents();
        ASSERT_NE(nullptr, side_panel_web_contents1);

        // Activate the second tab, verify the second side panel WebContents is
        // created for the second tab.
        browser()->tab_strip_model()->ActivateTabAt(1);
        content::WebContents* side_panel_web_contents2 =
            coordinator->GetActiveWebContents();
        ASSERT_NE(nullptr, side_panel_web_contents2);
        ASSERT_NE(side_panel_web_contents1, side_panel_web_contents2);

        // Activate the first tab, verify the active side panel WebContents is
        // swapped back.
        browser()->tab_strip_model()->ActivateTabAt(0);
        ASSERT_EQ(side_panel_web_contents1,
                  coordinator->GetActiveWebContents());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelPreserveOpenState) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify the side panel is open for thread1.
        EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(
                         browser()->tab_strip_model()->GetActiveWebContents()));
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Activate tab1, verify the side panel is open for thread2.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          browser()->tab_strip_model()->ActivateTabAt(1);
          EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

          histogram_tester.ExpectUniqueSample(
              "ContextualTasks.TabChange.UserAction.ChangedThreads", true, 1);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.ChangedThreads"),
                    1);
        }

        // Activate tab0. Verify the side panel is open for thread1.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          browser()->tab_strip_model()->ActivateTabAt(0);
          EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

          histogram_tester.ExpectUniqueSample(
              "ContextualTasks.TabChange.UserAction.ChangedThreads", true, 1);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.ChangedThreads"),
                    1);
        }

        // Close side panel for tab0, verify the side panel is closed for
        // thread1.
        coordinator->Close();
        EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

        // Activate tab1, verify the side panel is open for thread2.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          browser()->tab_strip_model()->ActivateTabAt(1);
          EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

          histogram_tester.ExpectUniqueSample(
              "ContextualTasks.TabChange.UserAction.OpenSidePanel", true, 1);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.OpenSidePanel"),
                    1);
        }

        // Activate tab2, verify the active side panel is closed for thread1.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          browser()->tab_strip_model()->ActivateTabAt(2);
          EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

          histogram_tester.ExpectUniqueSample(
              "ContextualTasks.TabChange.UserAction.CloseSidePanel", true, 1);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.CloseSidePanel"),
                    1);
        }

        // Activate tab0, verify the active side panel is closed for thread1.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          browser()->tab_strip_model()->ActivateTabAt(0);
          EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

          // No tab change histograms should be recorded as this is the status
          // quo.
          histogram_tester.ExpectTotalCount(
              "ContextualTasks.TabChange.UserAction.CloseSidePanel", 0);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.CloseSidePanel"),
                    0);
          histogram_tester.ExpectTotalCount(
              "ContextualTasks.TabChange.UserAction.OpenSidePanel", 0);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.CloseSidePanel"),
                    0);
          histogram_tester.ExpectTotalCount(
              "ContextualTasks.TabChange.UserAction.StayedOnThread", 0);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.StayedOnThread"),
                    0);
          histogram_tester.ExpectTotalCount(
              "ContextualTasks.TabChange.UserAction.ChangedThreads", 0);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.ChangedThreads"),
                    0);
        }

        // Show side panel for tab0, verify the side panel is open for thread1.
        coordinator->Show();
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Show side panel for tab2, verify the side panel is open for thread1.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          browser()->tab_strip_model()->ActivateTabAt(2);
          EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

          histogram_tester.ExpectUniqueSample(
              "ContextualTasks.TabChange.UserAction.StayedOnThread", true, 1);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.StayedOnThread"),
                    1);
        }

        // Activate tab3, verify the side panel is closed with no associated
        // thread.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          browser()->tab_strip_model()->ActivateTabAt(3);
          EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

          histogram_tester.ExpectUniqueSample(
              "ContextualTasks.TabChange.UserAction.CloseSidePanel", true, 1);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.CloseSidePanel"),
                    1);
        }
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelOpenWithTabWithoutTask) {
  SetUpTasks();
  // Add a new foreground tab not associated with a task.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, true);
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify the side panel can still open.
        ASSERT_NE(nullptr, coordinator->GetActiveWebContents());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelOpenByTransferWebContentsFromTab) {
  SetUpTasks();
  // Add tab4 with contextual task side panel tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIContextualTasksURL), -1,
                   true);
  int detach_index = tab_strip_model->GetIndexOfWebContents(
      tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(4, detach_index);
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());

  content::WebContents* tab_web_contents;
  ContextualTask task3 = contextual_tasks_controller->CreateTask();

  // Associate tab_web_contents to task3.
  contextual_tasks_controller->AssociateTabWithTask(
      task3.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                             tab_strip_model->GetActiveWebContents()));

  RunTestSequence(
      Do([&]() {
        // Add tab5. Create a new task and associate with it.
        chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1,
                         true);
        int current_index = tab_strip_model->GetIndexOfWebContents(
            tab_strip_model->GetActiveWebContents());
        EXPECT_EQ(5, current_index);
        contextual_tasks_controller->AssociateTabWithTask(
            task3.GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                tab_strip_model->GetWebContentsAt(current_index)));

        // Transfer the WebContents from tab 4 to the side panel.
        std::unique_ptr<content::WebContents> contextual_task_contents =
            tab_strip_model->DetachWebContentsAtForInsertion(
                detach_index,
                TabStripModelChange::RemoveReason::kInsertedIntoSidePanel);
        tab_web_contents = contextual_task_contents.get();

        coordinator->TransferWebContentsFromTab(
            task3.GetTaskId(), std::move(contextual_task_contents));
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify there are 5 tabs in the tab strip.
        EXPECT_EQ(5, tab_strip_model->count());

        // Verify the tab web contents is transferred into the side panel.
        EXPECT_EQ(tab_web_contents, coordinator->GetActiveWebContents());

        // Verify the tab web contents is still associated with task3.
        EXPECT_TRUE(contextual_tasks_controller->GetContextualTaskForTab(
            sessions::SessionTabHelper::IdForTab(tab_web_contents)));
      }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorInteractiveUiTest,
    SidePanelOpenByTranferWebContentsFromTab_HistoryCleared) {
  SetUpTasks();
  // Add tab4 that will eventually move to the side panel.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIVersionURL), -1, true);
  int detach_index = tab_strip_model->GetIndexOfWebContents(
      tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(4, detach_index);

  // Navigate the tab a few times to create a back stack. Make sure to end on
  // the contextual tasks URL.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));
  EXPECT_EQ(
      3,
      tab_strip_model->GetActiveWebContents()->GetController().GetEntryCount());

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());

  content::WebContents* tab_web_contents;
  ContextualTask task3 = contextual_tasks_controller->CreateTask();

  // Associate tab_web_contents to task3.
  contextual_tasks_controller->AssociateTabWithTask(
      task3.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                             tab_strip_model->GetActiveWebContents()));

  RunTestSequence(
      Do([&]() {
        // Add tab5. Create a new task and associate with it.
        chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1,
                         true);
        int current_index = tab_strip_model->GetIndexOfWebContents(
            tab_strip_model->GetActiveWebContents());
        EXPECT_EQ(5, current_index);
        contextual_tasks_controller->AssociateTabWithTask(
            task3.GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                tab_strip_model->GetWebContentsAt(current_index)));

        // Transfer the WebContents from tab 4 to the side panel.
        std::unique_ptr<content::WebContents> contextual_task_contents =
            tab_strip_model->DetachWebContentsAtForInsertion(
                detach_index,
                TabStripModelChange::RemoveReason::kInsertedIntoSidePanel);
        tab_web_contents = contextual_task_contents.get();

        coordinator->TransferWebContentsFromTab(
            task3.GetTaskId(), std::move(contextual_task_contents));
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify there are 5 tabs in the tab strip.
        EXPECT_EQ(5, tab_strip_model->count());

        // Verify the tab web contents is transferred into the side panel.
        EXPECT_EQ(tab_web_contents, coordinator->GetActiveWebContents());

        // Moving the WebContents to the side panel should also clear the back
        // stack.
        EXPECT_EQ(1, tab_web_contents->GetController().GetEntryCount());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelCreateNewTask) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        content::WebContents* web_contents1 =
            coordinator->GetActiveWebContents();
        // Change current task from task1 to a new task.
        ContextualTasksContextController* contextual_tasks_controller =
            ContextualTasksContextControllerFactory::GetForProfile(
                browser()->profile());
        ContextualTask new_task = contextual_tasks_controller->CreateTask();
        contextual_tasks_controller->AssociateTabWithTask(
            new_task.GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetActiveWebContents()));
        coordinator->OnTaskChanged(web_contents1, new_task.GetTaskId());
        EXPECT_TRUE(coordinator->IsSidePanelOpen());

        // Activate tab1, it associates with the task2 WebContents.
        browser()->tab_strip_model()->ActivateTabAt(1);
        EXPECT_NE(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsSidePanelOpen());

        // Activate tab0, it associates with the new WebContents.
        browser()->tab_strip_model()->ActivateTabAt(0);
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsSidePanelOpen());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelSelectExistingTask) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        content::WebContents* web_contents1 =
            coordinator->GetActiveWebContents();
        // Change current task from task1 to task2.
        ContextualTasksContextController* contextual_tasks_controller =
            ContextualTasksContextControllerFactory::GetForProfile(
                browser()->profile());
        contextual_tasks_controller->AssociateTabWithTask(
            task_id2_,
            sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetActiveWebContents()));
        coordinator->OnTaskChanged(web_contents1, task_id2_);
        EXPECT_TRUE(coordinator->IsSidePanelOpen());

        // Activate tab1, now it associates with the current WebContents.
        browser()->tab_strip_model()->ActivateTabAt(1);
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsSidePanelOpen());

        // Activate tab0, it still associates with the current WebContents.
        browser()->tab_strip_model()->ActivateTabAt(0);
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsSidePanelOpen());
      }));
}

// TODO(crbug.com/470086449): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       DISABLED_UpdateActiveTabContextStatusOnTabSwitch) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  GURL foo("https://foo.com");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), foo));
  coordinator->Show();
  ContextualTasksUI* ui = GetContextualTasksUI();
  mojo::PendingRemote<composebox::mojom::Page> composebox_page_remote;
  mojo::PendingReceiver<composebox::mojom::Page> composebox_page_receiver =
      composebox_page_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<composebox::mojom::PageHandler> composebox_handler_remote;
  mojo::PendingReceiver<composebox::mojom::PageHandler>
      composebox_handler_receiver =
          composebox_handler_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page_remote;
  mojo::PendingReceiver<searchbox::mojom::Page> searchbox_page_receiver =
      searchbox_page_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<searchbox::mojom::PageHandler> searchbox_handler_remote;
  mojo::PendingReceiver<searchbox::mojom::PageHandler>
      searchbox_handler_receiver =
          searchbox_handler_remote.InitWithNewPipeAndPassReceiver();

  auto mock_composebox_handler =
      std::make_unique<testing::NiceMock<MockContextualTasksComposeboxHandler>>(
          ui, browser()->profile(),
          browser()->tab_strip_model()->GetWebContentsAt(0),
          std::move(composebox_handler_receiver),
          std::move(composebox_page_remote),
          std::move(searchbox_handler_receiver));
  MockContextualTasksComposeboxHandler* mock_handler =
      mock_composebox_handler.get();
  ui->SetComposeboxHandlerForTesting(std::move(mock_composebox_handler));
  coordinator->Close();

  // Define expectations on the mock handler.
  using TabInfo = searchbox::mojom::TabInfo;

  // Expectations are set before running the sequence.
  // This should trigger UpdateSuggestedTabContext with valid tab info.
  EXPECT_CALL(*mock_handler,
              UpdateSuggestedTabContext(Pointee(Field(&TabInfo::url, foo))))
      .Times(1);

  RunTestSequence(
      // 1. Open side panel.
      Do([&]() { coordinator->Show(); }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      // Verify that `OnActiveTabContextStatusChanged` is called on the UI.
      Check([&]() {
        Mock::VerifyAndClearExpectations(mock_handler);
        // Set next expectation. Because the other tab has a chrome:// URL,
        // `UpdateSuggestedTabContext` will be called with a nullptr.
        searchbox::mojom::TabInfoPtr null_tab_info;
        EXPECT_CALL(*mock_handler,
                    UpdateSuggestedTabContext(IsNullSuggestedTabContext()))
            .Times(1);
        return true;
      }),
      // 2. Switch tabs to another tab.
      Do([&]() {
        browser()->tab_strip_model()->ActivateTabAt(2);
        ui->SetComposeboxHandlerForTesting(nullptr);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       CloseTabsCleanUpSidePanel) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        content::WebContents* web_contents1 =
            coordinator->GetActiveWebContents();
        ContextualTasksContextController* contextual_tasks_controller =
            ContextualTasksContextControllerFactory::GetForProfile(
                browser()->profile());

        SessionID tab_id0 = sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(0));

        // Close tab0, verify tab0 is removed from task1.
        EXPECT_EQ(task_id1_,
                  contextual_tasks_controller->GetContextualTaskForTab(tab_id0)
                      ->GetTaskId());
        browser()->tab_strip_model()->CloseWebContentsAt(
            0, TabCloseTypes::CLOSE_NONE);
        EXPECT_EQ(
            std::nullopt,
            contextual_tasks_controller->GetContextualTaskForTab(tab_id0));

        // Activate tab1, verify the side panel cache is still present.
        browser()->tab_strip_model()->ActivateTabAt(1);
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());

        SessionID tab_id1 = sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(1));

        // Close tab1, verify tab1 is removed from task1 and side panel
        // WebContents is removed.
        EXPECT_EQ(task_id1_,
                  contextual_tasks_controller->GetContextualTaskForTab(tab_id1)
                      ->GetTaskId());
        browser()->tab_strip_model()->CloseWebContentsAt(
            1, TabCloseTypes::CLOSE_NONE);
        EXPECT_EQ(
            std::nullopt,
            contextual_tasks_controller->GetContextualTaskForTab(tab_id1));

        EXPECT_EQ(nullptr, coordinator->GetActiveWebContents());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       OpenNewTabWithoutLinkClick_DoesNotInheritsOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new tab. The opener of tab4 is set to
  // tab1.
  browser()->tab_strip_model()->ActivateTabAt(1);
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
  EXPECT_EQ(5, browser()->tab_strip_model()->count());

  // Tab4 will not inherit the task from tab1 as it is not created through link
  // click.
  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());
  std::optional<ContextualTask> task1 =
      contextual_tasks_controller->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              browser()->tab_strip_model()->GetWebContentsAt(1)));
  std::optional<ContextualTask> task1_2 =
      contextual_tasks_controller->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              browser()->tab_strip_model()->GetWebContentsAt(4)));
  ASSERT_TRUE(task1);
  ASSERT_FALSE(task1_2);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       OpenNewTabWithLinkClick_InheritsOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new tab through link click.
  browser()->tab_strip_model()->ActivateTabAt(1);
  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kChromeUISettingsURL),
                                ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(5, browser()->tab_strip_model()->count());

  // Since tab1 is associated with task1, verify tab 2 is associated with the
  // same task.
  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());
  std::optional<ContextualTask> task1 =
      contextual_tasks_controller->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              browser()->tab_strip_model()->GetWebContentsAt(1)));
  std::optional<ContextualTask> task1_2 =
      contextual_tasks_controller->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              browser()->tab_strip_model()->GetWebContentsAt(2)));
  ASSERT_TRUE(task1);
  ASSERT_TRUE(task1_2);
  ASSERT_EQ(task1->GetTaskId(), task1_2->GetTaskId());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       MoveTabToNewWindowKeepTaskAssociation) {
  SetUpTasks();
  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());

  // Verify tab0 is associated to a task.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  std::optional<ContextualTask> task1 =
      contextual_tasks_controller->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(web_contents));
  ASSERT_TRUE(task1.has_value());

  // Move tab 0 to a new window.
  browser()->tab_strip_model()->delegate()->MoveTabsToNewWindow({0});

  // Verify tab0 is still associated to the same task.
  std::optional<ContextualTask> task2 =
      contextual_tasks_controller->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(web_contents));
  ASSERT_TRUE(task2.has_value());
  ASSERT_EQ(task1->GetTaskId(), task2->GetTaskId());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       NavigateToContextualTasksPageHidesSidePanel) {
  SetUpTasks();

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());

  // Show side panel.
  coordinator->Show();
  EXPECT_TRUE(coordinator->IsSidePanelOpenForContextualTask());

  // Navigate to a contextual tasks URL closes the side panel.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));
  EXPECT_FALSE(coordinator->IsSidePanelOpenForContextualTask());
}

class TabScopedContextualTasksSidePanelCoordinatorInteractiveUiTest
    : public ContextualTasksSidePanelCoordinatorInteractiveUiTest {
 public:
  TabScopedContextualTasksSidePanelCoordinatorInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kContextualTasks, {{"TaskScopedSidePanel", "false"}});
  }
  ~TabScopedContextualTasksSidePanelCoordinatorInteractiveUiTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TabScopedContextualTasksSidePanelCoordinatorInteractiveUiTest,
    SwitchTabChangeSidePanelWebContents) {
  SetUpTasks();

  ContextualTasksSidePanelCoordinator* coordinator =
      ContextualTasksSidePanelCoordinator::From(browser());
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show();
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify the first side panel WebContents is created for the first tab.
        content::WebContents* side_panel_web_contents1 =
            coordinator->GetActiveWebContents();
        ASSERT_NE(nullptr, side_panel_web_contents1);
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Activate the second tab, verify the second side panel WebContents is
        // created for the second tab.
        browser()->tab_strip_model()->ActivateTabAt(1);
        content::WebContents* side_panel_web_contents2 =
            coordinator->GetActiveWebContents();
        ASSERT_NE(nullptr, side_panel_web_contents2);
        ASSERT_NE(side_panel_web_contents1, side_panel_web_contents2);
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Activate the third tab, verify the active side panel WebContents is
        // swapped back.
        browser()->tab_strip_model()->ActivateTabAt(2);
        ASSERT_EQ(side_panel_web_contents1,
                  coordinator->GetActiveWebContents());
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Close the side panel for the third tab.
        coordinator->Close();
        EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

        // Switch back to first tab, verify the side panel is still open because
        // the open state is tab scoped.
        browser()->tab_strip_model()->ActivateTabAt(0);
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());
      }));
}

}  // namespace contextual_tasks
