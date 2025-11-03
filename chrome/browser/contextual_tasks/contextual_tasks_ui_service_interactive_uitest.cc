// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
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

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceInteractiveUiTest,
                       OnThreadLinkClicked_CreatesNewTabAndAssociates) {
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
        service->OnThreadLinkClicked(clicked_url, task1.GetTaskId(), nullptr);

        content::WebContents* new_content =
            browser()->tab_strip_model()->GetWebContentsAt(2);
        SessionID new_session_Id =
            sessions::SessionTabHelper::IdForTab(new_content);

        // Verify that the captured SessionID is valid and matches the new tab.
        EXPECT_TRUE(new_session_Id.is_valid());
        std::optional<ContextualTask> associated_task =
            contextual_tasks_controller->GetContextualTaskForTab(
                new_session_Id);
        EXPECT_TRUE(associated_task);
        EXPECT_EQ(associated_task->GetTaskId(), task1.GetTaskId());
      }));
}
}  // namespace contextual_tasks
