// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_search_session_finder.h"

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

contextual_search::ContextualSearchSessionHandle* FindSessionForTask(
    const base::Uuid& task_id,
    ContextualTasksService* contextual_tasks_service,
    BrowserWindowInterface* browser_window,
    ContextualTasksSidePanelCoordinator* side_panel_coordinator) {
  if (!contextual_tasks_service) {
    return nullptr;
  }

  if (side_panel_coordinator) {
    for (content::WebContents* web_contents :
         side_panel_coordinator->GetSidePanelWebContentsList()) {
      if (auto* helper = ContextualSearchWebContentsHelper::FromWebContents(
              web_contents)) {
        auto* existing_session = helper->GetSessionForTask(task_id);
        if (existing_session) {
          return existing_session;
        }
      }
    }
  }

  TabStripModel* tab_strip_model = browser_window->GetTabStripModel();
  if (tab_strip_model) {
    for (auto tab_id :
         contextual_tasks_service->GetTabsAssociatedWithTask(task_id)) {
      for (int i = 0; i < tab_strip_model->count(); ++i) {
        tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(i);
        if (sessions::SessionTabHelper::IdForTab(tab->GetContents()) ==
            tab_id) {
          if (auto* helper = ContextualSearchWebContentsHelper::FromWebContents(
                  tab->GetContents())) {
            auto* existing_session = helper->GetSessionForTask(task_id);
            if (existing_session) {
              return existing_session;
            }
          }
        }
      }
    }
  }
  return nullptr;
}

}  // namespace contextual_tasks
