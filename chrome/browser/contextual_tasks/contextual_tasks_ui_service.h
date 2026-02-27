// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/base/backoff_entry.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "url/gurl.h"

class AimEligibilityService;
class BrowserWindowInterface;
class GoogleServiceAuthError;
class Profile;

namespace base {
class Uuid;
}  // namespace base

namespace content {
struct OpenURLParams;
class WebContents;
}  // namespace content

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {
class ContextualTasksService;
class ContextualTasksUIInterface;

// A service used to coordinate all of the side panel instances showing an AI
// thread. Events like tab switching and Intercepted navigations from both the
// sidepanel and omnibox will be routed here.
class ContextualTasksUiService : public KeyedService {
 public:
  ContextualTasksUiService(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service);
  ContextualTasksUiService(const ContextualTasksUiService&) = delete;
  ContextualTasksUiService operator=(const ContextualTasksUiService&) = delete;
  ~ContextualTasksUiService() override;

  // KeyedService:
  void Shutdown() override;

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

  // A notification that a navigation to a link that is not related to the ai
  // thread occurred in the contextual tasks WebUI while being viewed in a tab
  // (as opposed to side panel).
  virtual void OnNonThreadNavigationInTab(
      const GURL& url,
      base::WeakPtr<tabs::TabInterface> tab);

  // A notification that a navigation to the search results page occurred in the
  // contextual tasks WebUI while being viewed in the side panel (as opposed to
  // a tab).
  virtual void OnSearchResultsNavigationInSidePanel(
      content::OpenURLParams url_params,
      ContextualTasksUIInterface* web_ui_interface);

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

  // Sets the entry point override for a given task.
  virtual void SetInitialEntryPointForTask(
      const base::Uuid& task_id,
      omnibox::ChromeAimEntryPoint entry_point);

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

  // Returns the URL for the default AI page for a given task.
  virtual GURL GetDefaultAiPageUrlForTask(const base::Uuid& task_id);
  // either in a full tab or in the side panel. If |task_id| is invalid, the
  // UI is in a zero-state that is waiting for user to create a new task.
  virtual void OnTaskChanged(BrowserWindowInterface* browser_window_interface,
                             content::WebContents* web_contents,
                             const base::Uuid& task_id,
                             bool is_shown_in_tab);

  // Opens the contextual tasks side panel and creates a new task with the given
  // URL as its initial thread URL.
  virtual void StartTaskUiInSidePanel(
      BrowserWindowInterface* browser_window_interface,
      tabs::TabInterface* tab_interface,
      const GURL& url,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  // Opens the contextual tasks side panel with the protected error page showing
  // by default.
  virtual void StartTaskUiInSidePanelWithErrorPage(
      BrowserWindowInterface* browser_window_interface,
      tabs::TabInterface* tab_interface,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  // Returns whether the provided URL is to an AI page.
  virtual bool IsAiUrl(const GURL& url);

  // Returns whether the provided task ID is for a task that should show the
  // error page on load.
  virtual bool IsPendingErrorPage(const base::Uuid& task_id);

  // Returns whether the provided URL is to a contextual tasks WebUI page.
  static bool IsContextualTasksUrl(const GURL& url);

  // Returns whether the provided URL represents a contextual tasks "display
  // URL" that should lead to the contextual tasks WebUI page upon navigation.
  bool IsContextualTasksDisplayUrl(const GURL& url);

  // Returns whether the provided URL is a Google search results page. This
  // method does not check for the validity of any parameters that
  // differentiate different modes or queries.
  static bool IsSearchResultsUrl(const GURL& url);

  // Returns whether the provided URL is a share URL.
  bool IsShareUrl(const GURL& url);

  // Returns whether the provided URL is for a valid (e.g. can be loaded in
  // the embedded page in the WebUI) search results page that contains the
  // correct params and isn't a shopping query.
  bool IsValidSearchResultsPage(const GURL& url);

  // Returns AIM URL found in the search param of the contextual tasks URL.
  // Returns empty URL if not found or not from AIM.
  static GURL GetAimUrlFromContextualTasksUrl(const GURL& url);

  // Called when the Lens overlay is shown/hidden. No-op if the active UI is not
  // in the side panel since the Lens button is always hidden in a tab.
  virtual void OnLensOverlayStateChanged(
      BrowserWindowInterface* browser_window_interface,
      bool is_showing,
      std::optional<lens::LensOverlayInvocationSource> invocation_source);

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
  virtual void OnTabClickedFromSourcesMenu(int32_t tab_id,
                                           const GURL& url,
                                           BrowserWindowInterface* browser);

  // Called when a file in the sources menu is clicked. Opens the file in a new
  // foreground tab.
  virtual void OnFileClickedFromSourcesMenu(const GURL& url,
                                            BrowserWindowInterface* browser);

  // Called when an image in the sources menu is clicked. Opens the image in a
  // new foreground tab.
  virtual void OnImageClickedFromSourcesMenu(const GURL& url,
                                             BrowserWindowInterface* browser);
  // Return whether there is a user signed into the browser with valid
  // credentials (aka, an OAuth token can be obtained).
  virtual bool IsSignedInToBrowserWithValidCredentials();

  // Return whether the cookie jar contains the primary account.
  virtual bool CookieJarContainsPrimaryAccount();

  // Fetches an access token for the primary account.
  using GetAccessTokenCallback = base::OnceCallback<void(const std::string&)>;
  virtual void GetAccessToken(
      GetAccessTokenCallback callback,
      base::WeakPtr<content::WebContents> web_contents);

  // Gets the entry point override for a given task.
  omnibox::ChromeAimEntryPoint GetInitialEntryPointForTask(
      const base::Uuid& task_id);

  base::WeakPtr<ContextualTasksUiService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
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

 private:
  void StartAccessTokenFetch();

  // Called when the OAuth token is received. If the token is valid, it is
  // passed to all pending access token callbacks. Otherwise, the fetch is
  // retried if the error is transient, or an empty token is passed to the
  // callbacks if the error is persistent.
  void OnOAuthTokenReceived(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  // Shows the OAuth error dialog for the given `web_contents`.
  void ShowOauthErrorDialogForWebContents(
      base::WeakPtr<content::WebContents> web_contents);

  // Runs all pending access token callbacks with the provided token.
  void RunPendingAccessTokenCallbacks(const std::string& token);

  // Focus an existing tab based on the provided URL if it exists. The URLs are
  // compared without text selection directives as they don't change the page
  // content and only tell the browser what text to highlight on the page. A
  // pointer to the selected tab is returned if found.
  // TODO(crbug.com/483442073): Remove the ifdef block once we remove
  // TabStripModel from MaybeFocusExistingOpenTab.
  tabs::TabInterface* MaybeFocusExistingOpenTab(const GURL& url,
                                                TabListInterface* tab_list,
                                                const base::Uuid& task_id);

  // A callback for checking whether text fragments from a URL are on a page.
  void OnTextFinderLookupComplete(
      base::WeakPtr<tabs::TabInterface> tab,
      const GURL& url,
      base::Uuid task_id,
      base::WeakPtr<BrowserWindowInterface> browser,
      const std::vector<std::pair<std::string, bool>>& lookup_results);

  // Helper method to associate the WebContents with the task and set the
  // session handle.
  void InitializeTaskInSidePanel(
      content::WebContents* web_contents,
      const base::Uuid& task_id,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  // Navigates to a share URL.
  virtual void OnShareUrlNavigation(const GURL& url);

  // Checks if the provided URL matches any of the allowed hosts.
  static bool IsAllowedHost(const GURL& url);

  const raw_ptr<Profile> profile_;

  const raw_ptr<contextual_tasks::ContextualTasksService>
      contextual_tasks_service_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ptr<AimEligibilityService> aim_eligibility_service_;

  // The access token fetcher for the current request.
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  // Pending access token callbacks.
  std::vector<
      std::pair<GetAccessTokenCallback, base::WeakPtr<content::WebContents>>>
      pending_access_token_callbacks_;

  // Backoff entry used to control the retry logic for the OAuth token request.
  net::BackoffEntry request_access_token_backoff_;

  // A timer used to refresh the OAuth token before it expires.
  base::OneShotTimer token_refresh_timer_;

  // Map a task's ID to the URL that was used to create it, if it exists. This
  // is primarily used in init flows where the contextual tasks UI is
  // intercepting a query from some other surface like the omnibox. The entry
  // in this map is removed once the UI is loaded with the correct thread.
  std::map<base::Uuid, GURL> task_id_to_creation_url_;

  // Map a task's ID to the entry point that was used to open it. This is used
  // to populate the aep param for GetInitialUrlForTask.
  // TODO(crbug.com/480176325): Clean the contents of the map when tasks
  // are cleaned up.
  std::map<base::Uuid, omnibox::ChromeAimEntryPoint>
      task_id_to_entry_point_override_;

  // Map of tasks that should show the error page on load to the source trigger.
  std::map<base::Uuid, contextual_search::ContextualSearchSource>
      pending_error_page_tasks_;

  base::WeakPtrFactory<ContextualTasksUiService> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
