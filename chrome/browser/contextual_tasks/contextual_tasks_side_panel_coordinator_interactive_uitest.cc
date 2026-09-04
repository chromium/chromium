// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/web_contents_tester.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/actions/actions.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget_deletion_observer.h"

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
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      GetSessionHandleCallback get_session_callback,
      ClearSessionHandleCallback clear_session_callback,
      TakeInputStateModelCallback get_inputstatemodel_callback)
      : ContextualTasksComposeboxHandler(
            ui_controller,
            profile,
            web_contents,
            std::move(pending_handler),
            std::move(pending_searchbox_handler),
            std::move(pending_searchbox_page),
            std::move(get_session_callback),
            std::move(clear_session_callback),
            std::move(get_inputstatemodel_callback)) {}
  ~MockContextualTasksComposeboxHandler() override = default;

  MOCK_METHOD(void,
              UpdateSuggestedTabContext,
              (const contextual_tasks::SuggestedTabInfo* suggested_tab),
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
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kContextualTasks, {}},
         {kContextualTasksForceEntryPointEligibility, {}},
         {kContextualTasksEphemeralBrandedEntryPoint,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
        {});
  }
  ~ContextualTasksSidePanelCoordinatorInteractiveUiTest() override = default;

  void SetPanelSuppressed(bool suppressed) {
    GetCoordinator()->SetPanelSuppressedForTesting(suppressed);
  }

  void SetUpTasks() {
    ActiveTaskContextProvider::From(browser())->AddObserver(
        &mock_active_task_context_provider_observer_);

    // Add tab1.
    chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
    // Add tab2.
    chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
    // Add tab3.
    chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

    ContextualTasksService* contextual_tasks_service =
        ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());

    // Create task1 and associate with tab0 and tab2, create task2 and associate
    // with tab1. Left tab3 with no task associated with.
    ContextualTask task1 = contextual_tasks_service->CreateTask();
    task_id1_ = task1.GetTaskId();
    contextual_tasks_service->AssociateTabWithTask(
        task1.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            TabListInterface::From(browser())->GetTab(0)->GetContents()));
    contextual_tasks_service->UpdateThreadForTask(
        task1.GetTaskId(), ThreadType::kAiMode, "thread1", std::nullopt,
        "Title 1");

    ContextualTask task2 = contextual_tasks_service->CreateTask();
    task_id2_ = task2.GetTaskId();
    contextual_tasks_service->AssociateTabWithTask(
        task2.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            TabListInterface::From(browser())->GetTab(1)->GetContents()));
    contextual_tasks_service->UpdateThreadForTask(
        task2.GetTaskId(), ThreadType::kAiMode, "thread2", std::nullopt,
        "Title 2");

    contextual_tasks_service->AssociateTabWithTask(
        task1.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            TabListInterface::From(browser())->GetTab(2)->GetContents()));

    // CachedWebContents are only created when transferring a tab to the side
    // panel or when calling Show(). Use the test-only method to imitate a
    // session where the side panel has been created for each of these tasks.
    ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
    coordinator->CreateCachedWebContentsForTesting(task_id1_, /*is_open=*/true);
    coordinator->CreateCachedWebContentsForTesting(task_id2_, /*is_open=*/true);

    browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) {
              const GURL& url = params->url_request.url;
              LOG(INFO) << "URLLoaderInterceptor intercepted URL: "
                        << url.spec();
              if (url.host() == "www.google.com") {
                content::URLLoaderInterceptor::WriteResponse(
                    "chrome/test/data/mock_aim_page.html",
                    params->client.get());
                return true;
              }
              return false;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  ContextualTasksUI* GetContextualTasksUI() {
    ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
    content::WebContents* web_contents = coordinator->GetActiveWebContents();
    if (!web_contents) {
      return nullptr;
    }
    ContextualTasksUI* ui = static_cast<ContextualTasksUI*>(
        web_contents->GetWebUI()->GetController());
    return ui;
  }

  ContextualTasksSidePanelCoordinator* GetCoordinator() {
    ContextualTasksPanelController* controller =
        ContextualTasksPanelController::From(browser());
    return static_cast<ContextualTasksSidePanelCoordinator*>(controller);
  }

 protected:
  base::Uuid task_id1_;
  base::Uuid task_id2_;
  MockActiveTaskContextProviderObserver
      mock_active_task_context_provider_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
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
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify the first side panel WebContents is created for the first tab.
        content::WebContents* side_panel_web_contents1 =
            coordinator->GetActiveWebContents();
        ASSERT_NE(nullptr, side_panel_web_contents1);

        // Activate the second tab, verify the second side panel WebContents is
        // created for the second tab.
        TabListInterface* tab_list = TabListInterface::From(browser());
        tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
        content::WebContents* side_panel_web_contents2 =
            coordinator->GetActiveWebContents();
        ASSERT_NE(nullptr, side_panel_web_contents2);
        ASSERT_NE(side_panel_web_contents1, side_panel_web_contents2);

        // Activate the first tab, verify the active side panel WebContents is
        // swapped back.
        tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
        ASSERT_EQ(side_panel_web_contents1,
                  coordinator->GetActiveWebContents());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelPreserveOpenState) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify the side panel is open for thread1.
        EXPECT_EQ(0, TabListInterface::From(browser())->GetActiveIndex());
        EXPECT_EQ(true, coordinator->IsPanelOpenForContextualTask());

        // Activate tab1, verify the side panel is open for thread2.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          TabListInterface* tab_list = TabListInterface::From(browser());
          tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
          EXPECT_EQ(true, coordinator->IsPanelOpenForContextualTask());

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

          TabListInterface* tab_list = TabListInterface::From(browser());
          tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
          EXPECT_EQ(true, coordinator->IsPanelOpenForContextualTask());

          histogram_tester.ExpectUniqueSample(
              "ContextualTasks.TabChange.UserAction.ChangedThreads", true, 1);
          EXPECT_EQ(user_action_tester.GetActionCount(
                        "ContextualTasks.TabChange.UserAction.ChangedThreads"),
                    1);
        }

        // Close side panel for tab0, verify the side panel is closed for
        // thread1.
        coordinator->Close();
        EXPECT_EQ(false, coordinator->IsPanelOpenForContextualTask());

        // Activate tab1, verify the side panel is open for thread2.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          TabListInterface* tab_list = TabListInterface::From(browser());
          tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
          EXPECT_EQ(true, coordinator->IsPanelOpenForContextualTask());

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

          TabListInterface* tab_list = TabListInterface::From(browser());
          tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
          EXPECT_EQ(false, coordinator->IsPanelOpenForContextualTask());

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

          TabListInterface* tab_list = TabListInterface::From(browser());
          tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
          EXPECT_EQ(false, coordinator->IsPanelOpenForContextualTask());

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
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
        EXPECT_EQ(true, coordinator->IsPanelOpenForContextualTask());

        // Show side panel for tab2, verify the side panel is open for thread1.
        {
          base::HistogramTester histogram_tester;
          base::UserActionTester user_action_tester;

          TabListInterface* tab_list = TabListInterface::From(browser());
          tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
          EXPECT_EQ(true, coordinator->IsPanelOpenForContextualTask());

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

          TabListInterface* tab_list = TabListInterface::From(browser());
          tab_list->ActivateTab(tab_list->GetTab(3)->GetHandle());
          EXPECT_EQ(false, coordinator->IsPanelOpenForContextualTask());

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
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify the side panel can still open.
        ASSERT_NE(nullptr, coordinator->GetActiveWebContents());
      }));
}

// TODO(crbug.com/478095504): Flakily fails on ASan/LSan
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SidePanelOpenByTransferWebContentsFromTab \
  DISABLED_SidePanelOpenByTransferWebContentsFromTab
#else
#define MAYBE_SidePanelOpenByTransferWebContentsFromTab \
  SidePanelOpenByTransferWebContentsFromTab
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       MAYBE_SidePanelOpenByTransferWebContentsFromTab) {
  SetUpTasks();
  // Add tab4 with contextual task side panel tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIContextualTasksURL), -1,
                   true);
  int detach_index = tab_strip_model->GetIndexOfWebContents(
      tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(4, detach_index);
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());

  content::WebContents* tab_web_contents;
  ContextualTask task3 = contextual_tasks_service->CreateTask();

  // Associate tab_web_contents to task3.
  contextual_tasks_service->AssociateTabWithTask(
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
        contextual_tasks_service->AssociateTabWithTask(
            task3.GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                tab_strip_model->GetWebContentsAt(current_index)));

        // Transfer the WebContents from tab 4 to the side panel.
        std::unique_ptr<content::WebContents> contextual_task_contents =
            tab_strip_model->DetachWebContentsAtForInsertion(
                detach_index, TabRemovedReason::kInsertedIntoSidePanel);
        tab_web_contents = contextual_task_contents.get();

        coordinator->TransferWebContentsFromTab(
            task3.GetTaskId(), std::move(contextual_task_contents));
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify there are 5 tabs in the tab strip.
        EXPECT_EQ(5, tab_strip_model->count());

        // Verify the tab web contents is transferred into the side panel.
        EXPECT_EQ(tab_web_contents, coordinator->GetActiveWebContents());

        // Verify the tab web contents is still associated with task3.
        EXPECT_TRUE(contextual_tasks_service->GetContextualTaskForTab(
            sessions::SessionTabHelper::IdForTab(tab_web_contents)));
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       DialogDelegateAddedOnTransferToSidePanel) {
  SetUpTasks();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // The test should be set up with 4 tabs.
  EXPECT_EQ(4, tab_strip_model->count());

  // Tab 0 with task 1 should be focused.
  int detach_index = tab_strip_model->GetIndexOfWebContents(
      tab_strip_model->GetActiveWebContents());
  EXPECT_EQ(0, detach_index);

  // Navigate to contextual tasks.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  content::WebContents* tab_web_contents;

  RunTestSequence(
      Do([&]() {
        // Select tab2 (also associated with task 1 in setup).
        tab_strip_model->ActivateTabAt(2);

        // Transfer the WebContents from tab 0 to the side panel.
        std::unique_ptr<content::WebContents> contextual_task_contents =
            tab_strip_model->DetachWebContentsAtForInsertion(
                detach_index, TabRemovedReason::kInsertedIntoSidePanel);
        tab_web_contents = contextual_task_contents.get();

        EXPECT_EQ(nullptr,
                  web_modal::WebContentsModalDialogManager::FromWebContents(
                      tab_web_contents)
                      ->delegate());

        coordinator->TransferWebContentsFromTab(
            task_id1_, std::move(contextual_task_contents));
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Verify there are now 3 tabs in the tab strip.
        EXPECT_EQ(3, tab_strip_model->count());

        // Verify the tab web contents is transferred into the side panel.
        EXPECT_EQ(tab_web_contents, coordinator->GetActiveWebContents());

        EXPECT_NE(nullptr,
                  web_modal::WebContentsModalDialogManager::FromWebContents(
                      tab_web_contents)
                      ->delegate());
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

  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());

  content::WebContents* tab_web_contents;
  ContextualTask task3 = contextual_tasks_service->CreateTask();

  // Associate tab_web_contents to task3.
  contextual_tasks_service->AssociateTabWithTask(
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
        contextual_tasks_service->AssociateTabWithTask(
            task3.GetTaskId(),
            sessions::SessionTabHelper::IdForTab(
                tab_strip_model->GetWebContentsAt(current_index)));

        // Transfer the WebContents from tab 4 to the side panel.
        std::unique_ptr<content::WebContents> contextual_task_contents =
            tab_strip_model->DetachWebContentsAtForInsertion(
                detach_index, TabRemovedReason::kInsertedIntoSidePanel);
        tab_web_contents = contextual_task_contents.get();

        coordinator->TransferWebContentsFromTab(
            task3.GetTaskId(), std::move(contextual_task_contents));
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
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
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        content::WebContents* web_contents1 =
            coordinator->GetActiveWebContents();
        TabListInterface* tab_list = TabListInterface::From(browser());
        // Change current task from task1 to a new task.
        ContextualTasksService* contextual_tasks_service =
            ContextualTasksServiceFactory::GetForProfile(
                browser()->GetProfile());
        ContextualTask new_task = contextual_tasks_service->CreateTask();
        contextual_tasks_service->AssociateTabWithTask(
            new_task.GetTaskId(), sessions::SessionTabHelper::IdForTab(
                                      tab_list->GetActiveTab()->GetContents()));
        coordinator->OnTaskChanged(web_contents1, new_task.GetTaskId());
        EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());

        // Activate tab1, it associates with the task2 WebContents.
        tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
        EXPECT_NE(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());

        // Activate tab0, it associates with the new WebContents.
        tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelSelectExistingTask) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        content::WebContents* web_contents1 =
            coordinator->GetActiveWebContents();
        TabListInterface* tab_list = TabListInterface::From(browser());
        // Change current task from task1 to task2.
        ContextualTasksService* contextual_tasks_service =
            ContextualTasksServiceFactory::GetForProfile(
                browser()->GetProfile());
        contextual_tasks_service->AssociateTabWithTask(
            task_id2_, sessions::SessionTabHelper::IdForTab(
                           tab_list->GetActiveTab()->GetContents()));
        coordinator->OnTaskChanged(web_contents1, task_id2_);
        EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());

        // Activate tab1, now it associates with the current WebContents.
        tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());

        // Activate tab0, it still associates with the current WebContents.
        tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());
        EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       UpdateActiveTabContextStatusOnTabSwitch) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  GURL foo("https://foo.com");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), foo));
  coordinator->Show(false,
                    omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
  ContextualTasksUI* ui = GetContextualTasksUI();
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
          ui, browser()->GetProfile(),
          TabListInterface::From(browser())->GetTab(0)->GetContents(),
          std::move(composebox_handler_receiver),
          std::move(searchbox_handler_receiver),
          std::move(searchbox_page_remote),
          base::BindRepeating(
              &ContextualTasksUI::GetOrCreateContextualSessionHandle,
              base::Unretained(ui)),
          base::DoNothing(),
          base::BindRepeating(&ContextualTasksUI::TakeInputStateModel,
                              base::Unretained(ui)));
  MockContextualTasksComposeboxHandler* mock_handler =
      mock_composebox_handler.get();
  ui->SetComposeboxHandlerForTesting(std::move(mock_composebox_handler));
  coordinator->Close();

  // Define expectations on the mock handler.
  using SuggestedTabInfo = contextual_tasks::SuggestedTabInfo;

  base::RunLoop initial_run_loop;
  EXPECT_CALL(*mock_handler, UpdateSuggestedTabContext(
                                 Pointee(Field(&SuggestedTabInfo::url, foo))))
      .WillRepeatedly(
          [&](const SuggestedTabInfo* tab_info) { initial_run_loop.Quit(); });

  base::RunLoop tab_switch_run_loop;
  EXPECT_CALL(*mock_handler, UpdateSuggestedTabContext(testing::IsNull()))
      .WillOnce([&]() { tab_switch_run_loop.Quit(); });

  RunTestSequence(
      // 1. Open side panel.
      Do([&]() {
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      // Trigger update on open now that panel is showing.
      Do([&]() { ui->OnActiveTabContextStatusChanged(); }),
      // Wait for UpdateSuggestedTabContext to be called with foo.
      Do([&]() { initial_run_loop.Run(); }),
      // Verify that active tab context suggestion is showing.
      Check([&]() { return ui->IsActiveTabContextSuggestionShowing(); }),
      // 2. Switch tabs to another tab.
      Do([&]() {
        TabListInterface* tab_list = TabListInterface::From(browser());
        tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
      }),
      // 3. Wait for UpdateSuggestedTabContext(nullptr) before resetting
      // handler.
      Do([&]() {
        tab_switch_run_loop.Run();
        EXPECT_FALSE(ui->IsActiveTabContextSuggestionShowing());
        ui->SetComposeboxHandlerForTesting(nullptr);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       CloseTabsCleanUpSidePanel) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        content::WebContents* web_contents1 =
            coordinator->GetActiveWebContents();
        ContextualTasksService* contextual_tasks_service =
            ContextualTasksServiceFactory::GetForProfile(
                browser()->GetProfile());

        TabListInterface* tab_list = TabListInterface::From(browser());
        SessionID tab_id0 = sessions::SessionTabHelper::IdForTab(
            tab_list->GetTab(0)->GetContents());

        // Close tab0, verify tab0 is removed from task1.
        EXPECT_EQ(task_id1_,
                  contextual_tasks_service->GetContextualTaskForTab(tab_id0)
                      ->GetTaskId());
        tab_list->CloseTab(tab_list->GetTab(0)->GetHandle());
        EXPECT_EQ(std::nullopt,
                  contextual_tasks_service->GetContextualTaskForTab(tab_id0));

        // Activate tab1, verify the side panel cache is still present.
        tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
        EXPECT_EQ(web_contents1, coordinator->GetActiveWebContents());

        SessionID tab_id1 = sessions::SessionTabHelper::IdForTab(
            tab_list->GetTab(1)->GetContents());

        // Close tab1, verify tab1 is removed from task1 and side panel
        // WebContents is removed.
        EXPECT_EQ(task_id1_,
                  contextual_tasks_service->GetContextualTaskForTab(tab_id1)
                      ->GetTaskId());
        tab_list->CloseTab(tab_list->GetTab(1)->GetHandle());
        EXPECT_EQ(std::nullopt,
                  contextual_tasks_service->GetContextualTaskForTab(tab_id1));

        EXPECT_EQ(nullptr, coordinator->GetActiveWebContents());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       CleanUpExpiredSidePanelCache) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  RunTestSequence(
      Do([&]() {
        // Open side panel.
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Switch to tab 1 ->task 2.
        TabListInterface* tab_list = TabListInterface::From(browser());
        tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
        content::WebContents* web_contents =
            coordinator->GetActiveWebContents();
        EXPECT_NE(nullptr, coordinator->GetActiveWebContents());
        // Switch to tab 0 -> task 1.
        tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
        EXPECT_NE(nullptr, coordinator->GetActiveWebContents());
        // Update timestamp of task 2 side panel WebContents to simulate
        // expiration.
        coordinator->GetWebContentsCacheItemForWebContents(web_contents)
            ->last_active_time_ticks =
            base::TimeTicks::Now() -
            base::Minutes(ContextualTasksInactiveSidePanelKeepInCacheMinutes() +
                          100);
        // Switch to tab 2 -> task 1. This should trigger logic to clean up the
        // side panel WebContents of task 2.
        tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
        EXPECT_NE(nullptr, coordinator->GetActiveWebContents());
        // Switch to tab 1, verify the side panel WebContents is no longer
        // there.
        tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
        EXPECT_EQ(nullptr, coordinator->GetActiveWebContents());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       OpenNewTabWithoutLinkClick_DoesNotInheritsOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new tab. The opener of tab4 is set to
  // tab1.
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Tab4 will not inherit the task from tab1 as it is not created through link
  // click.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  std::optional<ContextualTask> task1 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(1)->GetContents()));
  std::optional<ContextualTask> task1_2 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(4)->GetContents()));
  ASSERT_TRUE(task1);
  ASSERT_FALSE(task1_2);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorInteractiveUiTest,
    OpenNewBackgroundTabWithLinkClick_DoesNotInheritOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new background tab through link click.
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  NavigateParams params(browser(), GURL(chrome::kChromeUISettingsURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Verify tab 2 (background tab) does NOT inherit the task from tab 1.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  std::optional<ContextualTask> task1 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(1)->GetContents()));
  std::optional<ContextualTask> task1_2 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(2)->GetContents()));
  ASSERT_TRUE(task1);
  ASSERT_FALSE(task1_2);

  // Switch to the newly opened tab and verify the side panel is not open.
  tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       OpenNewTabWithLinkClick_InheritsOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new tab through link click.
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kChromeUISettingsURL),
                                ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Since tab1 is associated with task1, verify tab 2 is associated with the
  // same task.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  std::optional<ContextualTask> task1 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(1)->GetContents()));
  std::optional<ContextualTask> task1_2 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(2)->GetContents()));
  ASSERT_TRUE(task1);
  ASSERT_TRUE(task1_2);
  ASSERT_EQ(task1->GetTaskId(), task1_2->GetTaskId());
}

class ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest
    : public ContextualTasksSidePanelCoordinatorInteractiveUiTest {
 public:
  ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasksSidePanel},
        /*disabled_features=*/{kContextualTasks});
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest,
    OpenNewBackgroundTabWithLinkClick_ContextualTasksDisabled_DoesNotInheritOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new background tab through link click.
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  NavigateParams params(browser(), GURL(chrome::kChromeUISettingsURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  Navigate(&params);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Verify tab 2 (background tab) does NOT inherit the task from tab 1.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  std::optional<ContextualTask> task1 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(1)->GetContents()));
  std::optional<ContextualTask> task1_2 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(2)->GetContents()));
  ASSERT_TRUE(task1);
  ASSERT_FALSE(task1_2);

  // Switch to the newly opened tab and verify the side panel is not open.
  tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest,
    OpenNewForegroundTabWithLinkClick_ContextualTasksDisabled_InheritsOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new foreground tab through link click
  // (e.g. target="_blank").
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kChromeUISettingsURL),
                                ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Verify tab 2 (foreground tab) inherits the task from tab 1.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  std::optional<ContextualTask> task1 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(1)->GetContents()));
  std::optional<ContextualTask> task1_2 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(2)->GetContents()));
  ASSERT_TRUE(task1);
  ASSERT_TRUE(task1_2);
  ASSERT_EQ(task1->GetTaskId(), task1_2->GetTaskId());

  // Verify the side panel is open for the new foreground tab.
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest,
    OnThreadLinkClicked_ContextualTasksDisabled_AssociatesNewTab) {
  SetUpTasks();
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->GetProfile());
  ASSERT_TRUE(ui_service);

  ui_service->OnThreadLinkClicked(GURL(chrome::kChromeUISettingsURL), task_id2_,
                                  /*tab=*/nullptr, browser()->GetWeakPtr(),
                                  url::Origin());
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Verify the newly active tab opened from the panel is associated with
  // the task.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  content::WebContents* active_contents =
      tab_list->GetActiveTab()->GetContents();
  std::optional<ContextualTask> active_task =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(active_contents));
  ASSERT_TRUE(active_task.has_value());
  EXPECT_EQ(task_id2_, active_task->GetTaskId());

  // Verify the side panel is open for the new tab.
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       MoveTabToNewWindowKeepTaskAssociation) {
  SetUpTasks();
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());

  // Verify tab0 is associated to a task.
  content::WebContents* web_contents =
      TabListInterface::From(browser())->GetTab(0)->GetContents();
  std::optional<ContextualTask> task1 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(web_contents));
  ASSERT_TRUE(task1.has_value());

  // Move tab 0 to a new window.
  chrome::MoveTabsToNewWindow(browser(), {0});

  // Verify tab0 is still associated to the same task.
  std::optional<ContextualTask> task2 =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(web_contents));
  ASSERT_TRUE(task2.has_value());
  ASSERT_EQ(task1->GetTaskId(), task2->GetTaskId());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       DoNotOpenPanelWhenSuppressed) {
  SetUpTasks();

  TabListInterface* tab_list = TabListInterface::From(browser());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  // Show panel.
  coordinator->Show(false,
                    omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
  EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());

  // Show Customize Chrome side panel.
  chrome::ExecuteCommandWithContext(
      browser(), IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL,
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kToolbarButton))
          .Build());

  // Verify the panel is closed.
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());

  // Set the panel to be suppressed. This mimics the behavior where the glic
  // panel is open and suppresses the Contextual Tasks panel.
  SetPanelSuppressed(true);

  // Add a new foreground tab not associated with a task.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, true);

  // Verify the panel is closed.
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());

  // Activate the previous tab.
  // Verify the panel is still closed because it is suppressed.
  tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       NavigateToContextualTasksPageHidesSidePanel) {
  SetUpTasks();

  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  // Show side panel.
  coordinator->Show(false,
                    omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
  EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());

  // Navigate to a contextual tasks URL closes the side panel.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       WebContentsVisibilityChanged) {
  SetUpTasks();

  TabListInterface* tab_list = TabListInterface::From(browser());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  // Show side panel. Current WebContents is visible.
  coordinator->Show(false,
                    omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
  EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());
  content::WebContents* web_contents1 = coordinator->GetActiveWebContents();
  web_contents1->WasShown();
  EXPECT_EQ(content::Visibility::VISIBLE, web_contents1->GetVisibility());

  // Switch to tab1. Previous WebContents is hidden. Current WebContents is
  // visible.
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  content::WebContents* web_contents2 = coordinator->GetActiveWebContents();
  EXPECT_EQ(content::Visibility::HIDDEN, web_contents1->GetVisibility());
  EXPECT_EQ(content::Visibility::VISIBLE, web_contents2->GetVisibility());

  // Close the side panel. Both WebContents are hidden.
  coordinator->Close();
  EXPECT_EQ(content::Visibility::HIDDEN, web_contents1->GetVisibility());
  EXPECT_EQ(content::Visibility::HIDDEN, web_contents2->GetVisibility());
}

// Regression test for crbug.com/517906613: On a buggy build, trying to show the
// pending dialog during SetWebContents when the view's web_contents() is stale
// would result in a null host crash in the dialog manager.
IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       ModalDialogSwitchTabsDoesNotCrash) {
  SetUpTasks();
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  views::Widget* dialog_widget = nullptr;
  std::unique_ptr<views::WidgetDeletionObserver> deletion_observer;

  RunTestSequence(
      Do([&]() {
        // Open the side panel for the active tab (Tab 0).
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Show a modal dialog on the active side panel's WebContents.
        content::WebContents* side_panel_contents =
            coordinator->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);

        auto dialog_model = ui::DialogModel::Builder()
                                .SetTitle(u"Test Modal Dialog")
                                .AddOkButton(base::DoNothing())
                                .Build();
        dialog_widget = constrained_window::ShowWebModal(
            std::move(dialog_model), side_panel_contents);
        ASSERT_NE(dialog_widget, nullptr);
        deletion_observer =
            std::make_unique<views::WidgetDeletionObserver>(dialog_widget);

        // Switch to a tab with no associated task to close the side panel.
        // This destroys the side panel view, leaving the WebContents with the
        // pending dialog cached in the coordinator.
        TabListInterface* tab_list = TabListInterface::From(browser());
        tab_list->ActivateTab(tab_list->GetTab(3)->GetHandle());
      }),
      WaitForHide(kContextualTasksSidePanelWebViewElementId), Do([&]() {
        // Switch back to Tab 0. This recreates the side panel view and triggers
        // SetWebContents with the cached WebContents.
        TabListInterface* tab_list = TabListInterface::From(browser());
        tab_list->ActivateTab(tab_list->GetTab(0)->GetHandle());
      }),
      Do([&]() {
        // If we didn't crash, verify the side panel is open again.
        EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());

        // Cleanup the dialog widget if it's still alive.
        if (deletion_observer && deletion_observer->IsWidgetAlive()) {
          dialog_widget->CloseNow();
        }
      }));
}

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFrameLoadedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kComposeboxFocusedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kHasFinishedTopLevelNavigationEvent);

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       ComposeboxFocusOnBoundsUpdateWhenComposeboxHidden) {
  SetUpTasks();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  StateChange contextual_tasks_app_exists;
  contextual_tasks_app_exists.type = StateChange::Type::kExists;
  contextual_tasks_app_exists.where = {"contextual-tasks-app"};
  contextual_tasks_app_exists.event = kElementExistsEvent;

  StateChange frame_loaded;
  frame_loaded.type = StateChange::Type::kExistsAndConditionTrue;
  frame_loaded.where = {"contextual-tasks-app"};
  frame_loaded.test_function = "(app) => !app.isFrameLoading";
  frame_loaded.event = kFrameLoadedEvent;

  StateChange composebox_focused;
  composebox_focused.type = StateChange::Type::kExistsAndConditionTrue;
  composebox_focused.where = {"contextual-tasks-app"};
  composebox_focused.test_function =
      "(app) => app.shadowRoot.activeElement && "
      "app.shadowRoot.activeElement.id === 'composebox'";
  composebox_focused.event = kComposeboxFocusedEvent;

  StateChange has_finished_top_level_navigation;
  has_finished_top_level_navigation.type =
      StateChange::Type::kExistsAndConditionTrue;
  has_finished_top_level_navigation.where = {"contextual-tasks-app"};
  has_finished_top_level_navigation.test_function =
      "(app) => app.getHasFinishedTopLevelNavigationForTesting()";
  has_finished_top_level_navigation.event = kHasFinishedTopLevelNavigationEvent;

  RunTestSequence(
      Do([&]() {
        coordinator->Show(
            false, omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      FocusWebContents(kSidePanelWebContentsId),
      WaitForStateChange(kSidePanelWebContentsId, contextual_tasks_app_exists),
      Do([&]() {
        content::WebContents* side_panel_contents =
            coordinator->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        // Use Object.defineProperty to mock the app's state properties. This
        // freezes the values for the duration of the test and prevents
        // asynchronous Mojo callbacks or page-load event handlers from
        // overwriting them in the background, which would cause flakiness.
        EXPECT_TRUE(content::ExecJs(
            side_panel_contents,
            "(() => {"
            "  const app = document.querySelector('contextual-tasks-app');"
            "  Object.defineProperty(app, 'isErrorDialogVisible_', {"
            "    get() { return false; },"
            "    set() {}"
            "  });"
            "  Object.defineProperty(app, 'isAimEligible_', {"
            "    get() { return true; },"
            "    set() {}"
            "  });"
            "  Object.defineProperty(app, 'isZeroState_', {"
            "    get() { return false; },"
            "    set() {}"
            "  });"
            "  Object.defineProperty(app, 'isAiPage_', {"
            "    get() { return true; },"
            "    set() {}"
            "  });"
            "  Object.defineProperty(app, 'enableComposeboxJumpFix_', {"
            "    get() { return true; },"
            "    set() {}"
            "  });"
            "  Object.defineProperty(app, 'isInputHidden_', {"
            "    get() { return false; },"
            "    set() {}"
            "  });"
            "  Object.defineProperty(app, 'enableBasicMode_', {"
            "    get() { return false; },"
            "    set() {}"
            "  });"
            "  Object.defineProperty(app, 'inNlm_', {"
            "    get() { return false; },"
            "    set() {}"
            "  });"
            "  app.forcedComposeboxBounds_ = null;"
            "})()"));
      }),
      WaitForStateChange(kSidePanelWebContentsId,
                         has_finished_top_level_navigation),
      WaitForStateChange(kSidePanelWebContentsId, frame_loaded), Do([&]() {
        content::WebContents* side_panel_contents =
            coordinator->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        // Isolate jump fix visibility from unrelated initial load CSS gating.
        EXPECT_TRUE(content::ExecJs(
            side_panel_contents,
            "(async () => {"
            "  const app = document.querySelector('contextual-tasks-app');"
            "  app.setIsInitialFrameLoadForTesting(false);"
            "  await app.updateComplete;"
            "})()"));
        EXPECT_EQ(
            true,
            content::EvalJs(
                side_panel_contents,
                "(() => {"
                "  const app = document.querySelector('contextual-tasks-app');"
                "  return app.isComposeboxHidden_();"
                "})()"));
      }),
      FocusWebContents(kSidePanelWebContentsId), Do([&]() {
        content::WebContents* side_panel_contents =
            coordinator->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        EXPECT_TRUE(content::ExecJs(
            side_panel_contents,
            "(() => {"
            "  const app = document.querySelector('contextual-tasks-app');"
            "  const mockRect = {top: 10, left: 10, width: 200, height: "
            "50, right: 210, bottom: 60};"
            "  app.onInputPlateBoundsUpdateForTesting(mockRect, []);"
            "})()"));
      }),
      WaitForStateChange(kSidePanelWebContentsId, composebox_focused),
      Do([&]() {
        content::WebContents* side_panel_contents =
            coordinator->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        EXPECT_EQ(
            false,
            content::EvalJs(
                side_panel_contents,
                "(() => {"
                "  const app = document.querySelector('contextual-tasks-app');"
                "  return app.isComposeboxHidden_();"
                "})()"));
      }));
}

}  // namespace contextual_tasks
