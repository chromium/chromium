// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_SEARCH_SESSION_FINDER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_SEARCH_SESSION_FINDER_H_

#include "base/uuid.h"
#include "chrome/browser/ui/browser_navigator_params.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_search {
class ContextualSearchSessionHandle;
class ContextualSearchService;
}  // namespace contextual_search

namespace contextual_tasks {
class ContextualTasksService;
class ContextualTasksSidePanelCoordinator;

// Finds an existing contextual search session for a given task ID by checking
// all affiliated tabs and side panel WebContents.
contextual_search::ContextualSearchSessionHandle* FindSessionForTask(
    const base::Uuid& task_id,
    ContextualTasksService* contextual_tasks_service,
    BrowserWindowInterface* browser_window,
    ContextualTasksSidePanelCoordinator* side_panel_coordinator = nullptr);

// Helper method to set task ID and session handle on the
// ContextualSearchWebContentsHelper associated with the given `web_contents`.
// Must be invoked whenever a the thread associated with the `web_contents`.
// changes. Finds an existing session open in browser if possible. If not
// found, creates a new session.
void UpdateContextualSearchWebContentsHelperForTask(
    contextual_search::ContextualSearchService* contextual_search_service,
    BrowserWindowInterface* browser_window,
    ContextualTasksService* contextual_tasks_service,
    ContextualTasksSidePanelCoordinator* side_panel_coordinator,
    content::WebContents* web_contents,
    const base::Uuid& task_id);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_SEARCH_SESSION_FINDER_H_
