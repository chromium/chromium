// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"

namespace contextual_tasks {

class ContextualTasksSidePanelCoordinatorInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  ContextualTasksSidePanelCoordinatorInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(kContextualTasks);
  }
  ~ContextualTasksSidePanelCoordinatorInteractiveUiTest() override = default;

  void SetUpTasks() {
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
    contextual_tasks_controller->AssociateTabWithTask(
        task1.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(0)));
    ContextualTask task2 = contextual_tasks_controller->CreateTask();
    contextual_tasks_controller->AssociateTabWithTask(
        task2.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(1)));
    contextual_tasks_controller->AssociateTabWithTask(
        task1.GetTaskId(),
        sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetWebContentsAt(2)));

    browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
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
            coordinator->GetActiveWebContentsForTesting();
        ASSERT_NE(nullptr, side_panel_web_contents1);

        // Activate the second tab, verify the second side panel WebContents is
        // created for the second tab.
        browser()->tab_strip_model()->ActivateTabAt(1);
        content::WebContents* side_panel_web_contents2 =
            coordinator->GetActiveWebContentsForTesting();
        ASSERT_NE(nullptr, side_panel_web_contents2);
        ASSERT_NE(side_panel_web_contents1, side_panel_web_contents2);

        // Activate the first tab, verify the active side panel WebContents is
        // swapped back.
        browser()->tab_strip_model()->ActivateTabAt(0);
        ASSERT_EQ(side_panel_web_contents1,
                  coordinator->GetActiveWebContentsForTesting());
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
        browser()->tab_strip_model()->ActivateTabAt(1);
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Activate tab0. Verify the side panel is open for thread1.
        browser()->tab_strip_model()->ActivateTabAt(0);
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Close side panel for tab0, verify the side panel is closed for
        // thread1.
        coordinator->Close();
        EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

        // Activate tab1, verify the side panel is open for thread2.
        browser()->tab_strip_model()->ActivateTabAt(1);
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Activate tab2, verify the active side panel is closed for thread1.
        browser()->tab_strip_model()->ActivateTabAt(2);
        EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

        // Activate tab0, verify the active side panel is closed for thread1.
        browser()->tab_strip_model()->ActivateTabAt(0);
        EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());

        // Show side panel for tab0, verify the side panel is open for thread1.
        coordinator->Show();
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Show side panel for tab2, verify the side panel is open for thread1.
        browser()->tab_strip_model()->ActivateTabAt(0);
        EXPECT_EQ(true, coordinator->IsSidePanelOpenForContextualTask());

        // Activate tab3, verify the side panel is closed with no associated
        // thread.
        browser()->tab_strip_model()->ActivateTabAt(3);
        EXPECT_EQ(false, coordinator->IsSidePanelOpenForContextualTask());
      }));
}

}  // namespace contextual_tasks
