// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_OMNIBOX_QUERY_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_OMNIBOX_QUERY_HELPER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace content {
class WebContents;
}  // namespace content

namespace lens {
struct ContextualInputData;
}  // namespace lens

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {

class ContextualTasksUiService;

// Fulfills a contextual omnibox suggestion on Desktop Android without Lens:
// captures and uploads the active tab's page context in a contextual search
// session, creates an AI Mode search URL, and opens it in the Contextual Tasks
// side panel. Falls back to loading the suggestion URL in the tab whenever the
// contextual flow is unavailable. Scoped to the `WebContents` so in-flight
// callbacks are cancelled on navigation, tab teardown, or a subsequent query.
class ContextualTasksOmniboxQueryHelper
    : public content::WebContentsUserData<ContextualTasksOmniboxQueryHelper>,
      public content::WebContentsObserver {
 public:
  // Returns whether contextual omnibox suggestions may be requested and
  // fulfilled by this helper for `web_contents`: the "Ask Google" entry points
  // are eligible, the `omnibox_contextual_suggestions` parameter is on, and
  // the user allows sharing page content with Search.
  static bool IsEligible(content::WebContents* web_contents);

  // Starts the fulfillment flow for `suggestion_url` in `web_contents`. Loads
  // `suggestion_url` directly when `IsEligible(web_contents)` is false.
  static void Start(content::WebContents* web_contents,
                    const GURL& suggestion_url);

  ContextualTasksOmniboxQueryHelper(const ContextualTasksOmniboxQueryHelper&) =
      delete;
  ContextualTasksOmniboxQueryHelper& operator=(
      const ContextualTasksOmniboxQueryHelper&) = delete;
  ~ContextualTasksOmniboxQueryHelper() override;

 private:
  friend class content::WebContentsUserData<ContextualTasksOmniboxQueryHelper>;

  explicit ContextualTasksOmniboxQueryHelper(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  void StartQuery(const GURL& suggestion_url);
  // Returns false if the contextual flow could not be started.
  bool TryStartContextualQuery();
  void OnTabContextCaptured(
      std::unique_ptr<lens::ContextualInputData> page_content_data);
  void CreateSearchUrl();
  void OnSearchUrlCreated(GURL url);
  ContextualTasksUiService* GetSidePanelUiService();
  static void LoadInTab(content::WebContents* web_contents, const GURL& url);
  void LoadInTabAndReset(const GURL& url);
  void Reset();

  base::WeakPtr<tabs::TabInterface> tab_;
  GURL suggestion_url_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  // Identifies the in-flight page context request so it can be cancelled.
  base::UnguessableToken page_context_request_id_;

  base::WeakPtrFactory<ContextualTasksOmniboxQueryHelper> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_OMNIBOX_QUERY_HELPER_H_
