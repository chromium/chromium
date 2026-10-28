// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include "base/check_deref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_onboarding_tooltip_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget_deletion_observer.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/extensions/reload_page_dialog_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/views_switches.h"
#endif

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
        // These tests expect the side panel state to be shared by the tabs in a
        // task.
        {kContextualTasksSidePanelTabState});
  }
  ~ContextualTasksSidePanelCoordinatorInteractiveUiTest() override = default;

  void SetPanelSuppressed(bool suppressed) {
    GetCoordinator()->SetPanelSuppressedForTesting(suppressed);
  }

  void HidePanel() { GetCoordinator()->Hide(); }

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

    SidePanelUI::From(browser())->DisableAnimationsForTesting();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    SidePanelUI::From(browser())->DisableAnimationsForTesting();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [&](content::URLLoaderInterceptor::RequestParams* params) {
              const GURL& url = params->url_request.url;
              LOG(INFO) << "URLLoaderInterceptor intercepted URL: "
                        << url.spec();
              if (url.host() == "www.google.com" ||
                  url.host() == "support.google.com") {
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

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SidePanelOpenByTransferWebContentsFromTab) {
  SetUpTasks();
  // Add tab4 with contextual task side panel tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUIContextualTasksURL), -1,
                   true);
  ASSERT_TRUE(
      content::WaitForLoadStop(tab_strip_model->GetActiveWebContents()));
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

// TODO(crbug.com/577969962): Flaky / timing out across multiple platforms.
IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       DISABLED_UpdateActiveTabContextStatusOnTabSwitch) {
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
  coordinator->Close();
  ui->SetComposeboxHandlerForTesting(std::move(mock_composebox_handler));

  // Define expectations on the mock handler.
  using SuggestedTabInfo = contextual_tasks::SuggestedTabInfo;

  base::RunLoop initial_run_loop;
  EXPECT_CALL(*mock_handler, UpdateSuggestedTabContext(
                                 Pointee(Field(&SuggestedTabInfo::url, foo))))
      .WillRepeatedly([&initial_run_loop](const SuggestedTabInfo* tab_info) {
        initial_run_loop.Quit();
      });

  bool switched_tabs = false;
  base::RunLoop tab_switch_run_loop;
  EXPECT_CALL(*mock_handler, UpdateSuggestedTabContext(testing::IsNull()))
      .WillRepeatedly([&switched_tabs, &tab_switch_run_loop](
                          const SuggestedTabInfo* tab_info) {
        if (switched_tabs) {
          tab_switch_run_loop.Quit();
        }
      });

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
        switched_tabs = true;
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

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       OpenNewBackgroundTabWithLinkClick_InheritsOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new background tab through link click.
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  NavigateParams params(browser(), GURL(chrome::kChromeUISettingsURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::kNewBackgroundTab;
  Navigate(&params);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Verify tab 2 (background tab) inherits the task from tab 1.
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

  // Switch to the newly opened tab and verify the side panel remains open.
  tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());
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

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorInteractiveUiTest,
    OpenNewTabWithLinkClick_WhenPanelClosed_DoesNotInheritOpenerTask) {
  SetUpTasks();
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  ASSERT_TRUE(coordinator->IsPanelOpenForContextualTask());

  // Close the side panel on tab1 (under ephemeral button mode, tab1 remains
  // associated with task1).
  coordinator->Close();
  ASSERT_FALSE(coordinator->IsPanelOpenForContextualTask());

  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kChromeUISettingsURL),
                                ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Because the side panel was closed on tab1, tab2 must NOT inherit task1.
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
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorInteractiveUiTest,
    OpenNewTabWithLinkClick_WhenPanelHidden_DoesNotInheritOpenerTask) {
  SetUpTasks();
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  ASSERT_TRUE(coordinator->IsPanelOpenForContextualTask());

  // Hide the side panel without closing the task (e.g. as happens when viewing
  // a full-tab Contextual Tasks page, where cache_item->is_open remains true).
  HidePanel();
  ASSERT_FALSE(coordinator->IsPanelOpenForContextualTask());

  chrome::AddSelectedTabWithURL(browser(), GURL(chrome::kChromeUISettingsURL),
                                ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Because the side panel was hidden on tab1, tab2 must NOT inherit task1 and
  // must NOT automatically open the side panel.
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
  EXPECT_FALSE(coordinator->IsPanelOpenForContextualTask());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorInteractiveUiTest,
    OpenUrlFromOmniboxEverywhere_DoesNotInheritOpenerTask) {
  SetUpTasks();
  // Set tab1 (which is associated with a contextual task and open side panel)
  // as the active tab.
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  ASSERT_EQ(4, tab_list->GetTabCount());

  OmniboxEverywhereService omnibox_everywhere_service(browser()->GetProfile());
  omnibox_everywhere_service.OpenUrl(GURL(chrome::kChromeUISettingsURL),
                                     WindowOpenDisposition::kCurrentTab,
                                     ui::PAGE_TRANSITION_GENERATED);
  EXPECT_EQ(5, tab_list->GetTabCount());

  tabs::TabInterface* new_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(new_tab);
  EXPECT_NE(tab_list->GetTab(1), new_tab);

  // Because the navigation uses PAGE_TRANSITION_GENERATED rather than
  // PAGE_TRANSITION_LINK, the newly opened tab must not inherit the previous
  // tab's contextual task or side panel.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  std::optional<ContextualTask> opener_task =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(
              tab_list->GetTab(1)->GetContents()));
  std::optional<ContextualTask> new_tab_task =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(new_tab->GetContents()));
  EXPECT_TRUE(opener_task.has_value());
  EXPECT_FALSE(new_tab_task.has_value());
  EXPECT_FALSE(GetCoordinator()->IsPanelOpenForContextualTask());
}

class ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest
    : public ContextualTasksSidePanelCoordinatorInteractiveUiTest {
 public:
  ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasksSidePanel},
        /*disabled_features=*/{kContextualTasks,
                               kContextualTasksClobberActiveTab});
  }
};

class ContextualTasksSidePanelCoordinatorFeatureDisabledClobberInteractiveUiTest
    : public ContextualTasksSidePanelCoordinatorInteractiveUiTest {
 public:
  ContextualTasksSidePanelCoordinatorFeatureDisabledClobberInteractiveUiTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasksSidePanel,
                              kContextualTasksClobberActiveTab},
        /*disabled_features=*/{kContextualTasks});
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorFeatureDisabledInteractiveUiTest,
    OpenNewBackgroundTabWithLinkClick_ContextualTasksDisabled_InheritsOpenerTask) {
  SetUpTasks();
  // Set tab1 as active tab and create a new background tab through link click.
  TabListInterface* tab_list = TabListInterface::From(browser());
  tab_list->ActivateTab(tab_list->GetTab(1)->GetHandle());
  NavigateParams params(browser(), GURL(chrome::kChromeUISettingsURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::kNewBackgroundTab;
  Navigate(&params);
  EXPECT_EQ(5, tab_list->GetTabCount());

  // Verify tab 2 (background tab) inherits the task from tab 1.
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

  // Switch to the newly opened tab and verify the side panel remains open.
  tab_list->ActivateTab(tab_list->GetTab(2)->GetHandle());
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  EXPECT_TRUE(coordinator->IsPanelOpenForContextualTask());
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

IN_PROC_BROWSER_TEST_F(
    ContextualTasksSidePanelCoordinatorFeatureDisabledClobberInteractiveUiTest,
    OnThreadLinkClicked_ContextualTasksDisabled_ClobbersActiveTabAndAssociatesTask) {
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
  EXPECT_EQ(4, tab_list->GetTabCount());

  // Verify the clobbered active tab is associated with the task.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  content::WebContents* active_contents =
      tab_list->GetActiveTab()->GetContents();
  std::optional<ContextualTask> active_task =
      contextual_tasks_service->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(active_contents));
  ASSERT_TRUE(active_task.has_value());
  EXPECT_EQ(task_id2_, active_task->GetTaskId());

  // Verify the side panel is open for the active tab.
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

class ContextualTasksSignedOutAlignmentInteractiveUiTest
    : public ContextualTasksInteractiveTestBase {
 public:
  ContextualTasksSignedOutAlignmentInteractiveUiTest() = default;
  ~ContextualTasksSignedOutAlignmentInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    for (const auto& feature : GetDefaultEnabledFeatures()) {
      if (feature.feature == lens::features::kLensSidePanelUnification) {
        base::FieldTrialParams params = feature.params;
        params["allow-signed-out"] = "true";
        enabled_features.emplace_back(lens::features::kLensSidePanelUnification,
                                      params);
      } else {
        enabled_features.push_back(feature);
      }
    }
    std::vector<base::test::FeatureRef> disabled_features =
        GetDefaultDisabledFeatures();
    disabled_features.push_back(kContextualTasksForceEntryPointEligibility);
    disabled_features.push_back(kEnableContextualTasksPinButtonInToolbar);
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  std::unique_ptr<KeyedService> BuildMockContextualTasksUiServiceInstance(
      content::BrowserContext* context) override {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<TestContextualTasksUiService>(
        profile, ContextualTasksServiceFactory::GetForProfile(profile),
        AimEligibilityServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile),
        /*is_signed_in=*/false);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksSignedOutAlignmentInteractiveUiTest,
                       SignedOutUserCanChangeSidePanelAlignmentInSettings) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTabId);
  const WebContentsInteractionTestUtil::DeepQuery
      kContextualTasksAlignmentSelect = {
          "settings-ui",
          "settings-main",
          "settings-appearance-page-index",
          "settings-appearance-page",
          "settings-dropdown-menu[data-entry-id=\"kContextualTasks\"]",
          "select#dropdownMenu"};

  Profile* profile = browser()->GetProfile();
  ASSERT_FALSE(EntryPointEligibilityManager::IsPinningEligible(profile));
  auto* eligibility_manager =
      ContextualTasksEligibilityManager::GetForProfile(profile);
  ASSERT_TRUE(eligibility_manager &&
              eligibility_manager->IsSidePanelAvailable());

  actions::ActionItem* root_action_item =
      BrowserActions::From(browser())->root_action_item();
  actions::ActionItem* contextual_tasks_action =
      actions::ActionManager::Get().FindAction(
          kActionSidePanelShowContextualTasks, root_action_item);
  ASSERT_NE(contextual_tasks_action, nullptr);
  EXPECT_FALSE(contextual_tasks_action->GetVisible());

  SidePanel* side_panel =
      BrowserView::GetBrowserViewForBrowser(browser())->side_panel();
  side_panel->UpdateHorizontalAlignment(SidePanelEntryId::kContextualTasks);
  EXPECT_EQ(side_panel->horizontal_alignment(),
            SidePanel::HorizontalAlignment::kLeft);

  RunTestSequence(
      InstrumentTab(kSettingsTabId),
      NavigateWebContents(kSettingsTabId, GURL("chrome://settings/appearance")),
      WaitForJsResultAt(kSettingsTabId, kContextualTasksAlignmentSelect,
                        "el => !!el"),
      ScrollIntoView(kSettingsTabId, kContextualTasksAlignmentSelect),
      WaitForElementVisible(kSettingsTabId, kContextualTasksAlignmentSelect),
      CheckJsResultAt(kSettingsTabId, kContextualTasksAlignmentSelect,
                      "el => el.value", "false"),
      ExecuteJsAt(kSettingsTabId, kContextualTasksAlignmentSelect,
                  "el => { el.value = 'true'; el.dispatchEvent(new "
                  "Event('change')); }"),
      CheckResult(
          [&]() {
            const base::DictValue& overrides = profile->GetPrefs()->GetDict(
                prefs::kSidePanelAlignmentOverrides);
            return overrides
                .FindBool(SidePanelEntryIdToString(
                    SidePanelEntryId::kContextualTasks))
                .value_or(false);
          },
          true),
      Do([&]() {
        side_panel->UpdateHorizontalAlignment(
            SidePanelEntryId::kContextualTasks);
        EXPECT_EQ(side_panel->horizontal_alignment(),
                  SidePanel::HorizontalAlignment::kRight);
      }));
}

class ContextualTasksSignedOutPinnedButtonInteractiveUiTest
    : public ContextualTasksSignedOutAlignmentInteractiveUiTest {
 public:
  void SetUpFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    for (const auto& feature : GetDefaultEnabledFeatures()) {
      if (feature.feature == lens::features::kLensSidePanelUnification) {
        base::FieldTrialParams params = feature.params;
        params["allow-signed-out"] = "true";
        enabled_features.emplace_back(lens::features::kLensSidePanelUnification,
                                      params);
      } else {
        enabled_features.push_back(feature);
      }
    }
    enabled_features.push_back({kEnableContextualTasksPinButtonInToolbar, {}});
    std::vector<base::test::FeatureRef> disabled_features =
        GetDefaultDisabledFeatures();
    disabled_features.push_back(kContextualTasksForceEntryPointEligibility);
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksSignedOutPinnedButtonInteractiveUiTest,
                       SignedOutOpenInZeroState_DoesNotRedirectAndShowsToolbar) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
  const WebContentsInteractionTestUtil::DeepQuery kTopToolbarQuery = {
      "contextual-tasks-app", "top-toolbar#toolbar"};

  Profile* profile = browser()->GetProfile();
  EXPECT_TRUE(EntryPointEligibilityManager::IsPinningEligible(profile));

  auto* coordinator = ContextualTasksPanelController::From(browser());
  ASSERT_NE(coordinator, nullptr);
  RunTestSequence(
      Do([&]() { coordinator->OpenInZeroState(); }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              "SidePanelContentWebViewName"),
      WaitForJsResultAt(kSidePanelWebContentsId, kTopToolbarQuery,
                        "el => !!el"),
      Do([&]() {
        content::WebContents* side_panel_contents =
            coordinator->GetActiveWebContents();
        ASSERT_NE(side_panel_contents, nullptr);
        EXPECT_TRUE(ContextualTasksUiService::IsContextualTasksUrl(
            side_panel_contents->GetLastCommittedURL()));
      }));
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
class ContextualTasksExtensionsInteractiveUiTest
    : public ContextualTasksInteractiveTestBaseT<UserVariation> {
 public:
  ContextualTasksExtensionsInteractiveUiTest() = default;
  ~ContextualTasksExtensionsInteractiveUiTest() override = default;

  void SetUpFeatureList() override {
    auto enabled = GetDefaultEnabledFeatures();
    enabled.push_back({kContextualTasksRearchitecture, {}});
    enabled.push_back({kContextualTasksSidePanelRearchitecture, {}});
    enabled.push_back({extensions_features::kExtensionsMenuAccessControl, {}});
    feature_list_.InitWithFeaturesAndParameters(enabled,
                                                GetDefaultDisabledFeatures());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextualTasksInteractiveTestBaseT<UserVariation>::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch(
        views::switches::kDisableInputEventActivationProtectionForTesting);
  }

  void SetUpOnMainThread() override {
    ContextualTasksInteractiveTestBaseT<UserVariation>::SetUpOnMainThread();
    SidePanelUI::From(browser())->DisableAnimationsForTesting();
  }

  const extensions::Extension* LoadExtensionWithContentScript() {
    auto dir = std::make_unique<extensions::TestExtensionDir>();
    constexpr char kManifest[] = R"({
      "name": "Side Panel Reload Test Extension",
      "version": "1.0",
      "manifest_version": 3,
      "host_permissions": ["<all_urls>"],
      "content_scripts": [{
        "matches": ["<all_urls>"],
        "js": ["script.js"],
        "run_at": "document_end"
      }]
    })";
    dir->WriteManifest(kManifest);
    dir->WriteFile(FILE_PATH_LITERAL("script.js"), "console.log('injected');");
    extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
    loader.set_allow_incognito_access(true);
    loader.set_grant_permissions(true);
    scoped_refptr<const extensions::Extension> extension =
        loader.LoadExtension(dir->UnpackedPath());
    test_extension_dirs_.push_back(std::move(dir));
    return extension.get();
  }

 private:
  std::vector<std::unique_ptr<extensions::TestExtensionDir>>
      test_extension_dirs_;
};

// This tests the following CUJ:
//  (1) User installs an extension with content scripts and opens a Google
//      Search tab.
//  (2) User opens the Lens Overlay from the app menu and submits a query in
//      the overlay searchbox to open the Contextual Tasks side panel.
//  (3) User clicks the Super G button in the Contextual Tasks side panel
//      toolbar to open the Page Info bubble, then clicks the "Extensions"
//      button to open the side panel extensions menu.
//  (4) User toggles off the extension's site access in the side panel
//      extensions menu.
//  (5) The reload page dialog bubble pops up anchored in the Contextual Tasks
//      side panel (with TOP_LEFT arrow alignment), rather than on the main
//      browser toolbar.
//  (6) User clicks the Reload button in the dialog, reloading the Contextual
//      Tasks side panel WebContents without reloading the active browser tab.
IN_PROC_BROWSER_TEST_P(ContextualTasksExtensionsInteractiveUiTest,
                       ReloadPageDialogAnchoredToSidePanelAndReloadsPanel) {
  ASSERT_TRUE(LoadExtensionWithContentScript());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelToolbarWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelContentWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSuperGButtonReadyEvent);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSidePanelContentLoadedEvent);

  constexpr char kExtensionSiteAccessToggle[] = "extension_site_access_toggle";
  const GURL kMainTabUrl("https://www.google.com/search?q=main_tab");
  const GURL kSidePanelQueryUrl(
      "https://www.google.com/search?q=first+query&udm=50");

  const DeepQuery kPathToOverlaySearchboxInput{
      "lens-overlay-app",
      "cr-lens-searchbox",
      "cr-searchbox-input",
      "input",
  };
  const DeepQuery kPathToSuperGButton{
      "contextual-tasks-toolbar-app",
      "#toolbar",
      ".top-toolbar-logo-button",
  };

  StateChange super_g_button_ready;
  super_g_button_ready.type = StateChange::Type::kExistsAndConditionTrue;
  super_g_button_ready.where = kPathToSuperGButton;
  super_g_button_ready.test_function =
      "(el) => el.getBoundingClientRect().width > 0 && "
      "el.getBoundingClientRect().height > 0";
  super_g_button_ready.event = kSuperGButtonReadyEvent;

  StateChange side_panel_content_loaded;
  side_panel_content_loaded.type = StateChange::Type::kExistsAndConditionTrue;
  side_panel_content_loaded.where = {"body"};
  side_panel_content_loaded.test_function =
      "() => window.location.href.includes('udm=50') && "
      "document.readyState === 'complete'";
  side_panel_content_loaded.event = kSidePanelContentLoadedEvent;

  RunTestSequence(
      InstrumentTab(kPrimaryTab), NavigateWebContents(kPrimaryTab, kMainTabUrl),
      WaitForWebContentsPainted(kPrimaryTab),

      // Open the 3-dot App Menu and select the Lens Overlay option.
      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kShowLensOverlay),
      SelectMenuItem(AppMenuModel::kShowLensOverlay),

      // Wait for the Lens Overlay to load and submit a query to open the
      // Contextual Tasks side panel.
      InAnyContext(
          InstrumentNonTabWebView(kOverlayId,
                                  LensOverlayController::kOverlayId),
          WaitForWebContentsReady(
              kOverlayId, GURL(chrome::kChromeUILensOverlayUntrustedURL))),
      InSameContext(
          WaitForShow(LensOverlayController::kOverlayId), Do([&]() {
            content::WebContents* web_contents =
                browser()->tab_strip_model()->GetActiveWebContents();
            auto* lens_controller =
                LensSearchController::FromTabWebContents(web_contents);
            auto* query_router = static_cast<lens::FakeLensQueryFlowRouter*>(
                lens_controller->query_router());
            CHECK(query_router);
            auto* session_handle = static_cast<
                contextual_search::MockContextualSearchSessionHandle*>(
                query_router->GetContextualSearchSessionHandle());
            CHECK(session_handle);
            ON_CALL(*session_handle,
                    CreateSearchUrl(::testing::_, ::testing::_))
                .WillByDefault(::testing::WithArg<1>(
                    [kSidePanelQueryUrl](
                        base::OnceCallback<void(GURL)> callback) {
                      std::move(callback).Run(kSidePanelQueryUrl);
                    }));
          }),
          EnsurePresent(kOverlayId, kPathToOverlaySearchboxInput),
          FocusWebContents(kOverlayId),
          ExecuteJsAt(kOverlayId, kPathToOverlaySearchboxInput,
                      "(el) => { el.focus(); }",
                      ExecuteJsMode::kWaitForCompletion),
          ExecuteJsAt(
              kOverlayId, kPathToOverlaySearchboxInput,
              "(el) => { el.value = 'first query'; el.dispatchEvent(new "
              "Event('input', { bubbles: true })); el.dispatchEvent(new "
              "Event('change', { bubbles: true })); }",
              ExecuteJsMode::kWaitForCompletion),
          ExecuteJsAt(kOverlayId, kPathToOverlaySearchboxInput,
                      "(el) => { el.dispatchEvent(new KeyboardEvent('keydown', "
                      "{ key: 'Enter', bubbles: true, cancelable: true, "
                      "composed: true })); }",
                      ExecuteJsMode::kFireAndForget)),

      // Wait for the Contextual Tasks side panel to show and instrument both
      // its toolbar and content WebViews.
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelToolbarWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->toolbar_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelToolbarWebContentsId,
                              "SidePanelToolbarWebViewName"),
      NameViewRelative(kContextualTasksSidePanelWebViewElementId,
                       "SidePanelContentWebViewName",
                       [](ContextualTasksWebView* web_view) -> views::View* {
                         return web_view->content_web_view();
                       }),
      InstrumentNonTabWebView(kSidePanelContentWebContentsId,
                              "SidePanelContentWebViewName"),

      // Wait for the side panel content WebContents to finish loading and set
      // markers on both the main tab and the side panel content.
      WaitForStateChange(kSidePanelContentWebContentsId,
                         side_panel_content_loaded),
      ExecuteJs(kPrimaryTab, "() => { window.mainTabMarker = 'alive'; }"),
      ExecuteJs(kSidePanelContentWebContentsId,
                "() => { window.sidePanelMarker = 'before_reload'; }"),

      // Click the Super G button in the side panel toolbar to open Page Info,
      // then click "Extensions" to open the side panel extensions menu.
      InAnyContext(WaitForShow(kContextualTasksSuperGButtonElementId)),
      WaitForStateChange(kSidePanelToolbarWebContentsId, super_g_button_ready),
      ClickElement(kSidePanelToolbarWebContentsId, kPathToSuperGButton),
      WaitForShow(PageInfoMainView::kSeeExtensionsButtonElementId),
      PressButton(PageInfoMainView::kSeeExtensionsButtonElementId),

      // Toggle off the extension's site access in the side panel extensions
      // menu.
      WaitForShow(kExtensionsMenuMainPageElementId),
      WaitForShow(kExtensionsMenuEntryViewElementId),
      NameDescendantViewByType<views::ToggleButton>(
          kExtensionsMenuEntryViewElementId, kExtensionSiteAccessToggle),
      PressButton(kExtensionSiteAccessToggle),

      // The extensions menu closes and the reload page dialog pops up anchored
      // in the side panel (TOP_LEFT arrow alignment), rather than on the main
      // browser toolbar.
      WaitForHide(kExtensionsMenuMainPageElementId),
      WaitForShow(extensions::kReloadPageDialogOkButtonElementId),
      CheckView(extensions::kReloadPageDialogOkButtonElementId,
                [](views::View* view) {
                  return view->GetWidget()
                             ->widget_delegate()
                             ->AsBubbleDialogDelegate()
                             ->arrow() == views::BubbleBorder::TOP_LEFT;
                }),
      Check([&]() {
        content::WebContents* side_panel_contents =
            ContextualTasksPanelController::From(browser())
                ->GetActiveWebContents();
        return side_panel_contents &&
               extensions::TabHelper::FromWebContents(side_panel_contents)
                   ->IsReloadRequired();
      }),

      // Click the Reload button in the dialog and verify that the side panel
      // WebContents reloads while the active tab does not.
      PressButton(extensions::kReloadPageDialogOkButtonElementId),
      WaitForHide(extensions::kReloadPageDialogOkButtonElementId),
      WaitForWebContentsNavigation(kSidePanelContentWebContentsId),
      WaitForStateChange(kSidePanelContentWebContentsId,
                         side_panel_content_loaded),
      CheckJsResult(kSidePanelContentWebContentsId,
                    "() => window.sidePanelMarker === undefined"),
      CheckJsResult(kPrimaryTab, "() => window.mainTabMarker === 'alive'"),
      Check([&]() {
        content::WebContents* side_panel_contents =
            ContextualTasksPanelController::From(browser())
                ->GetActiveWebContents();
        return side_panel_contents &&
               !extensions::TabHelper::FromWebContents(side_panel_contents)
                    ->IsReloadRequired();
      }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksExtensionsInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         UserVariationToString);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

class ContextualTasksOnboardingBubbleInteractiveUiTest
    : public ContextualTasksSidePanelCoordinatorInteractiveUiTest {
 public:
  ContextualTasksOnboardingBubbleInteractiveUiTest() {
    onboarding_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasks, kContextualTasksContext,
                              kContextualTasksRearchitecture,
                              kContextualTasksSidePanelRearchitecture,
                              kContextualTasksShowOnboardingTooltip,
                              omnibox::kWebUIOmniboxAskGAboutThisPage},
        /*disabled_features=*/{});
  }

  ~ContextualTasksOnboardingBubbleInteractiveUiTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    ContextualTasksSidePanelCoordinatorInteractiveUiTest::
        SetUpBrowserContextKeyedServices(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
              CHECK_DEREF(profile->GetPrefs()),
              /*template_url_service=*/nullptr,
              /*url_loader_factory=*/nullptr,
              IdentityManagerFactory::GetForProfile(profile));
        }));
  }

  void ShowFaviconsElement() {
    ui::ElementContext context = BrowserElements::From(browser())->GetContext();
    views::Widget* browser_widget =
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
    test_element_ = std::make_unique<ui::test::TestElement>(
        kContextualTasksFaviconsElementId, context);
    test_element_->SetNativeView(browser_widget->GetNativeView());
    test_element_->SetScreenBounds(gfx::Rect(100, 100, 50, 50));
    test_element_->Show();
  }

  void AddAutomaticActiveTabToPanelSession() {
    auto* session_handle =
        GetCoordinator()->GetContextualSearchSessionHandleForPanel();
    ASSERT_NE(session_handle, nullptr);
    session_handle->AddPendingDelayedTab(1, GURL("https://example.com"),
                                         "Example");
  }

  void TearDownOnMainThread() override {
    test_element_.reset();
    ContextualTasksSidePanelCoordinatorInteractiveUiTest::
        TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList onboarding_feature_list_;
  std::unique_ptr<ui::test::TestElement> test_element_;
};

// This tests the following CUJ:
//  (1) User opens the Contextual Tasks side panel from the toolbar button.
//  (2) Tab favicons appear without an auto-suggested active tab, and the
//      onboarding bubble remains hidden until an auto-suggested active tab is
//      added.
//  (3) User clicks Got it, dismissing the bubble and incrementing the pref.
IN_PROC_BROWSER_TEST_F(
    ContextualTasksOnboardingBubbleInteractiveUiTest,
    ShowsOnboardingBubbleAnchoredToFaviconsAndDismissesOnGotIt) {
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  RunTestSequence(
      Do([coordinator]() {
        coordinator->Show(
            /*transition_from_tab=*/false,
            omnibox::ChromeAimEntryPoint::
                DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      Do([this]() { ShowFaviconsElement(); }),
      CheckResult(
          [coordinator]() {
            return coordinator->onboarding_controller_for_testing()
                ->IsBubbleShowing();
          },
          false),
      Do([this]() { AddAutomaticActiveTabToPanelSession(); }),
      WaitForShow(ContextualTasksOnboardingBubbleView::kBubbleElementId),
      CheckResult(
          [coordinator]() {
            return coordinator->onboarding_controller_for_testing()
                ->IsBubbleShowing();
          },
          true),
      CheckResult(
          [coordinator]() {
            return coordinator->onboarding_controller_for_testing()
                ->active_variant_for_testing();
          },
          ContextualTasksOnboardingBubbleView::Variant::kOnboarding),
      PressButton(ContextualTasksOnboardingBubbleView::kAcceptButtonElementId),
      WaitForHide(ContextualTasksOnboardingBubbleView::kBubbleElementId),
      CheckResult(
          [this]() {
            return browser()->GetProfile()->GetPrefs()->GetInteger(
                kContextualTasksOnboardingTooltipDismissedCount);
          },
          1));
}

// This tests the following CUJ:
//  (1) User opens the Contextual Tasks side panel via the omnibox Ask Google
//      action and tab favicons appear, showing the Ask Google privacy notice.
//  (2) User clicks the Learn more link in the bubble, opening the help tab
//      without incrementing the dismissed count pref.
//  (3) User clicks Got it, dismissing the bubble and incrementing the Ask
//      Google pref.
IN_PROC_BROWSER_TEST_F(
    ContextualTasksOnboardingBubbleInteractiveUiTest,
    ShowsAskGoogleBubbleForOmniboxActionAndLearnMoreOpensHelpTab) {
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  RunTestSequence(
      Do([coordinator]() {
        coordinator->Show(
            /*transition_from_tab=*/false,
            omnibox::ChromeAimEntryPoint::
                DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId),
      Do([this]() { ShowFaviconsElement(); }),
      WaitForShow(ContextualTasksOnboardingBubbleView::kBubbleElementId),
      CheckResult(
          [coordinator]() {
            return coordinator->onboarding_controller_for_testing()
                ->IsBubbleShowing();
          },
          true),
      CheckResult(
          [coordinator]() {
            return coordinator->onboarding_controller_for_testing()
                ->active_variant_for_testing();
          },
          ContextualTasksOnboardingBubbleView::Variant::kAskGoogle),
      WithElement(ContextualTasksOnboardingBubbleView::kBodyElementId,
                  [](ui::TrackedElement* el) {
                    auto* tracked_views = el->AsA<views::TrackedElementViews>();
                    ASSERT_NE(tracked_views, nullptr);
                    auto* label = views::AsViewClass<views::StyledLabel>(
                        tracked_views->view());
                    ASSERT_NE(label, nullptr);
                    label->ClickFirstLinkForTesting();
                  }),
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL();
          },
          GURL(GetContextualTasksTabHelpUrl())),
      CheckResult(
          [this]() {
            return browser()->GetProfile()->GetPrefs()->GetInteger(
                kContextualTasksAskGTooltipDismissedCount);
          },
          0),
      PressButton(ContextualTasksOnboardingBubbleView::kAcceptButtonElementId),
      WaitForHide(ContextualTasksOnboardingBubbleView::kBubbleElementId),
      CheckResult(
          [this]() {
            return browser()->GetProfile()->GetPrefs()->GetInteger(
                kContextualTasksAskGTooltipDismissedCount);
          },
          1));
}

// This tests the following CUJ:
//  (1) User opens the Contextual Tasks side panel and the onboarding bubble
//      appears anchored to the tab favicons element.
//  (2) User closes the Contextual Tasks side panel, automatically closing the
//      onboarding bubble.
IN_PROC_BROWSER_TEST_F(ContextualTasksOnboardingBubbleInteractiveUiTest,
                       ClosesOnboardingBubbleWhenSidePanelCloses) {
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();

  RunTestSequence(
      Do([coordinator]() {
        coordinator->Show(
            /*transition_from_tab=*/false,
            omnibox::ChromeAimEntryPoint::
                DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
      }),
      WaitForShow(kContextualTasksSidePanelWebViewElementId), Do([this]() {
        AddAutomaticActiveTabToPanelSession();
        ShowFaviconsElement();
      }),
      WaitForShow(ContextualTasksOnboardingBubbleView::kBubbleElementId),
      CheckResult(
          [coordinator]() {
            return coordinator->onboarding_controller_for_testing()
                ->IsBubbleShowing();
          },
          true),
      Do([coordinator]() { coordinator->Close(); }),
      WaitForHide(ContextualTasksOnboardingBubbleView::kBubbleElementId),
      CheckResult(
          [coordinator]() {
            return coordinator->onboarding_controller_for_testing()
                ->IsBubbleShowing();
          },
          false));
}

}  // namespace contextual_tasks
