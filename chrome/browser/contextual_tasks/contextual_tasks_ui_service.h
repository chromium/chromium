// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class ContextualTasksUI;
class Profile;

namespace base {
class Uuid;
}  // namespace base

namespace content {
struct OpenURLParams;
class WebContents;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {

class ContextualTasksContextController;

// A service used to coordinate all of the side panel instances showing an AI
// thread. Events like tab switching and Intercepted navigations from both the
// sidepanel and omnibox will be routed here.
class ContextualTasksUiService : public KeyedService {
 public:
  ContextualTasksUiService(Profile* profile,
                           ContextualTasksContextController* context_controller,
                           signin::IdentityManager* identity_manager);
  ContextualTasksUiService(const ContextualTasksUiService&) = delete;
  ContextualTasksUiService operator=(const ContextualTasksUiService&) = delete;
  ~ContextualTasksUiService() override;

  // A notification that the browser attempted to navigate to the AI page. If
  // this method is being called, it means the navigation was blocked and it
  // should be processed by this method.
  virtual void OnNavigationToAiPageIntercepted(
      const GURL& url,
      base::WeakPtr<tabs::TabInterface> source_tab,
      bool is_to_new_tab);

  // A notification to this service that a link in the AI thread was clicked by
  // the user. This will open a tab and associate it with the visible thread.
  virtual void OnThreadLinkClicked(
      const GURL& url,
      base::Uuid task_id,
      base::WeakPtr<tabs::TabInterface> tab,
      base::WeakPtr<BrowserWindowInterface> browser);

  // A notification that a navigation to the search results page occurred in the
  // contextual tasks WebUI while being viewed in a tab (as opposed to side
  // panel).
  virtual void OnSearchResultsNavigationInTab(
      const GURL& url,
      base::WeakPtr<tabs::TabInterface> tab);

  // A notification that a navigation to the search results page occurred in the
  // contextual tasks WebUI while being viewed in the side panel (as opposed to
  // a tab).
  virtual void OnSearchResultsNavigationInSidePanel(
      content::OpenURLParams url_params,
      ContextualTasksUI* webui_controller);

  // A notification that a navigation is occurring. This method gives the
  // service the opportunity to prevent the navigation from happening in
  // order to handle it manually. Returns true if the navigation is being
  // handled by the service (e.g. the navigation is blocked), and false
  // otherwise. The WebContents the navigation originated from is provided
  // along with `is_to_new_tab` which indicates whether the navigation would
  // open in a new tab or window. The `initiated_in_page` param is to help
  // determine if the navigation was from something like a link or redirect
  // versus an action in Chrome's UI like back/forward.
  virtual bool HandleNavigation(content::OpenURLParams url_params,
                                content::WebContents* source_contents,
                                bool is_from_embedded_page,
                                bool is_to_new_tab);

  // Returns the contextual_task UI for a task.
  virtual GURL GetContextualTaskUrlForTask(const base::Uuid& task_id);

  // Returns the URL that a task was created for. Once this is retrieved, the
  // entry is removed from the cache.
  virtual std::optional<GURL> GetInitialUrlForTask(const base::Uuid& uuid);

  // Get a thread URL based on the task ID. If no task is found or the task does
  // not have a thread ID, the default AI URL is returned.
  virtual void GetThreadUrlFromTaskId(const base::Uuid& task_id,
                                      base::OnceCallback<void(GURL)> callback);

  // Returns the URL for the default AI page. This is the URL that should be
  // loaded in the absence of any other context.
  virtual GURL GetDefaultAiPageUrl();

  // Called when the side panel in a given browser window started showing a new
  // task. If |task_id| is invalid, the panel is in a zero-state that is waiting
  // for user to create a new task.
  virtual void OnTaskChangedInPanel(
      BrowserWindowInterface* browser_window_interface,
      content::WebContents* web_contents,
      const base::Uuid& task_id);

  // Opens the contextual tasks side panel and creates a new task with the given
  // URL as its initial thread URL.
  virtual void StartTaskUiInSidePanel(
      BrowserWindowInterface* browser_window_interface,
      tabs::TabInterface* tab_interface,
      const GURL& url,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  // Returns whether the provided URL is to an AI page.
  bool IsAiUrl(const GURL& url);

  // Returns whether the provided URL is to a contextual tasks WebUI page.
  bool IsContextualTasksUrl(const GURL& url);

  // Returns whether the provided URL is for the search results page.
  bool IsSearchResultsPage(const GURL& url);

  // Associates a WebContents with a task, assuming the URL of the WebContents'
  // main frame or side panel is a contextual task URL.
  void AssociateWebContentsToTask(content::WebContents* web_contents,
                                  const base::Uuid& task_id);

  // Move the WebContents for the given task to a new tab.
  virtual void MoveTaskUiToNewTab(const base::Uuid& task_id,
                                  BrowserWindowInterface* browser,
                                  const GURL& inner_frame_url);

  // Called when a tab in the sources menu is clicked. Switches to the tab or
  // reopens the tab depending on whether the tab is already open on tab strip.
  void OnTabClickedFromSourcesMenu(int32_t tab_id,
                                   const GURL& url,
                                   BrowserWindowInterface* browser);

  void set_auto_tab_context_suggestion_enabled(bool enabled) {
    auto_tab_context_suggestion_enabled_ = enabled;
  }

  bool auto_tab_context_suggestion_enabled() const {
    return auto_tab_context_suggestion_enabled_;
  }

 protected:
  // The actual implementation of `HandleNavigation` that extracts more of the
  // components needed to decide if the navigation should be handled by this
  // service.
  virtual bool HandleNavigationImpl(content::OpenURLParams url_params,
                                    content::WebContents* source_contents,
                                    tabs::TabInterface* tab,
                                    bool is_from_embedded_page,
                                    bool is_to_new_tab);

  // Returns whether the provided URL is for the primary account in Chrome.
  virtual bool IsUrlForPrimaryAccount(const GURL& url);

  // Return whether there is a user is either signed into the browser or has
  // an account tied to the provided URL.
  virtual bool IsSignedInToWebOrBrowser(const GURL& url);

 private:
  const raw_ptr<Profile> profile_;

  raw_ptr<contextual_tasks::ContextualTasksContextController>
      context_controller_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  // The host of the AI page that is loaded into the WebUI.
  GURL ai_page_host_;

  // Map a task's ID to the URL that was used to create it, if it exists. This
  // is primarily used in init flows where the contextual tasks UI is
  // intercepting a query from some other surface like the omnibox. The entry
  // in this map is removed once the UI is loaded with the correct thread.
  std::map<base::Uuid, GURL> task_id_to_creation_url_;

  // Whether to allow active tab context to be suggested on compose box
  // automatically.
  bool auto_tab_context_suggestion_enabled_ = true;

  base::WeakPtrFactory<ContextualTasksUiService> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
