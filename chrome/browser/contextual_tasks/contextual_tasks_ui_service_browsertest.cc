// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include <vector>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

namespace contextual_tasks {

class ContextualTasksUiServiceBrowserTest : public InProcessBrowserTest {
 public:
  ContextualTasksUiServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(contextual_tasks::kContextualTasks);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that opening a citation for a URL that is already open highlights text
// on the page rather than opening a new tab.
IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceBrowserTest,
                       CitationTextHighlight_DoesNotOpenNewTab) {
  GURL url("data:text/html,<html><body>some text to highlight</body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  // Ensure the tab is open and on the expected URL.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  EXPECT_EQ(active_tab->GetContents()->GetVisibleURL(), url);

  // Create a task and associate the tab with the new task.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTask task = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  // Add a text fragment to the URL to mimic citation behavior.
  GURL citation_url = GURL(url.spec() + "#:~:text=highlight");

  // The text on the existing page should have highlights.
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetForPage(
          active_tab->GetContents()->GetPrimaryPage());
  EXPECT_EQ(nullptr, text_highlighter_manager);

  // Fake a thread link click without necessarily having to open the side panel
  // and negotiate all those moving pieces.
  ui_service->OnThreadLinkClicked(citation_url, task.GetTaskId(), nullptr,
                                  browser()->GetWeakPtr());

  // Wait for the TextHighlighterManager to be created.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    text_highlighter_manager = companion::TextHighlighterManager::GetForPage(
        active_tab->GetContents()->GetPrimaryPage());
    return text_highlighter_manager;
  }));

  // Ensure the tab count hasn't changed.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // There should now be a highlighter for the page.
  EXPECT_EQ(
      1u, text_highlighter_manager->get_text_highlighters_for_testing().size());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceBrowserTest,
                       CitationTextHighlight_TextNotFoundOpensTab) {
  GURL url("data:text/html,<html><body>some text to highlight</body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  // Ensure the tab is open and on the expected URL.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  EXPECT_EQ(active_tab->GetContents()->GetVisibleURL(), url);

  // Create a task and associate the tab with the new task.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTask task = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  // Add a text fragment to the URL that does not contain text found in the
  // page.
  GURL citation_url = GURL(url.spec() + "#:~:text=google");

  // The text on the existing page should have highlights.
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetForPage(
          active_tab->GetContents()->GetPrimaryPage());
  EXPECT_EQ(nullptr, text_highlighter_manager);

  // Fake a thread link click without necessarily having to open the side panel
  // and negotiate all those moving pieces.
  ui_service->OnThreadLinkClicked(citation_url, task.GetTaskId(), nullptr,
                                  browser()->GetWeakPtr());

  // Wait for the new tab to be created.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return browser()->tab_strip_model()->count() == 3; }));

  // A text highlighter should not have been created for the existing page.
  text_highlighter_manager = companion::TextHighlighterManager::GetForPage(
      active_tab->GetContents()->GetPrimaryPage());
  EXPECT_EQ(nullptr, text_highlighter_manager);

  // Another tab should have been opened since the text wasn't found.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
}

}  // namespace contextual_tasks
