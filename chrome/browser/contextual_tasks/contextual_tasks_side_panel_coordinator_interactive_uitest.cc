// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksSidePanelCoordinatorInteractiveUiTest,
                       SwitchTabChangeSidePanelWebContents) {
  // Add a second tab.
  chrome::AddTabAt(browser(), GURL(chrome::kChromeUISettingsURL), -1, false);

  ContextualTasksContextController* contextual_tasks_controller =
      ContextualTasksContextControllerFactory::GetForProfile(
          browser()->profile());

  // Create task1 and associate with tab0, create task2 and associate with tab1.
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

}  // namespace contextual_tasks
