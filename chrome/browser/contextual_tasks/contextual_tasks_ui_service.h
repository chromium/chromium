// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/base/backoff_entry.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "url/gurl.h"
#include "url/origin.h"

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
class RenderFrameHost;
struct GlobalRenderFrameHostToken;
}  // namespace content

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace lens {
class LensMediaLinkHandler;
}  // namespace lens

namespace contextual_tasks {
inline constexpr char kTaskQueryParam[] = "chrome_task_id";
inline constexpr char kChromeHostParam[] = "chrome_host";

class ContextualTasksCookieSynchronizer;
class ContextualTasksService;
class ContextualTasksUIInterface;
class ContextualTasksWindowTracker;
class ContextualTasksWindowTrackerManager;

// A service used to coordinate all of the side panel instances showing an AI
// thread. Events like tab switching and Intercepted navigations from both the
// sidepanel and omnibox will be routed here.
class ContextualTasksUiService : public KeyedService {
  FRIEND_TEST_ALL_PREFIXES(ContextualTasksUiServiceTest,
                           IsAllowedHost_WithOverride);
  FRIEND_TEST_ALL_PREFIXES(ContextualTasksUiServiceTest,
                           IsAllowedHost_LensDebugNotAllowed);

 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnContextualTasksUiServiceShutdown(
        ContextualTasksUiService* service) {}
  };

  ContextualTasksUiService(
      Profile* profile,
      std::unique_ptr<ContextualTasksUiServiceDelegate> delegate,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service,
      std::unique_ptr<ContextualTasksEligibilityManager> eligibility_manager,
      std::unique_ptr<ContextualTasksCookieSynchronizer> cookie_synchronizer);
  ContextualTasksUiService(const ContextualTasksUiService&) = delete;
  ContextualTasksUiService operator=(const ContextualTasksUiService&) = delete;
  ~ContextualTasksUiService() override;

  // KeyedService:
  void Shutdown() override;

  // Triggers the cookie synchronization to the isolated partition.
  virtual void EnsureCookiesSynced();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Registers a tracked window with its ID, associated task ID, and URL.
  void RegisterWindow(ContextualTaskId task_id,
                      const GURL& url,
                      ContextualWindowId window_id);

  // Requests the browser to close a tracked window.
  void CloseTrackedWindow(ContextualWindowId window_id);

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
      base::WeakPtr<BrowserWindowInterface> browser,
      const url::Origin& initiator_origin);

  // Determines if a new tab should be allowed to open for a thread link click.
  // Returns false if the link can be handled by focusing an existing tab or
  // scrolling a citation.
  bool ShouldAllowNewTabOpen(const GURL& url,
                             BrowserWindowInterface* browser,
                             const base::Uuid& task_id);

  // A notification that a navigation to a link that is not related to the ai
  // thread occurred in the contextual tasks WebUI while being viewed in a tab
  // (as opposed to side panel).
  virtual void OnNonThreadNavigationInTab(
      content::OpenURLParams url_params,
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
  virtual bool HandleNavigation(
      content::OpenURLParams url_params,
      content::WebContents* source_contents,
      bool is_from_embedded_page,
      bool from_can_create_window,
      bool is_same_site_or_from_ui,
      bool is_mobile_ua,
      const std::optional<url::Origin>& initiator_origin,
      const std::optional<content::GlobalRenderFrameHostToken>&
          initiator_frame_token);

  // Returns the contextual_task UI for a task.
  virtual GURL GetContextualTaskUrlForTask(const base::Uuid& task_id);

  // Sets the entry point override for a given task.
  virtual void SetInitialEntryPointForTask(
      const base::Uuid& task_id,
      omnibox::ChromeAimEntryPoint entry_point);

  // Returns the URL that a task was created for. Once this is retrieved, the
  // entry is removed from the cache.
  virtual std::optional<GURL> GetInitialUrlForTask(const base::Uuid& uuid);

  // Returns the URL that a task should be created for. This function does
  // not clear the entry from the cache.
  virtual std::optional<GURL> GetCreationUrlForTask(const base::Uuid& task_id);

  // Adds a callback to be run when the URL for a task becomes available.
  // This is only used in cases where the side panel is "warmed up" (i.e. using
  // very specific *GhostLoader methods).
  virtual void AddPendingUrlCallback(
      const base::Uuid& task_id,
      base::OnceCallback<void(const GURL&)> callback);

  // Returns whether the task is waiting for a URL to be generated.
  virtual bool IsTaskWaitingForUrl(const base::Uuid& task_id);

  // Get a thread URL based on the task ID. If no task is found or the task does
  // not have a thread ID, the default AI URL is returned.
  virtual void GetThreadUrlFromTaskId(const base::Uuid& task_id,
                                      base::OnceCallback<void(GURL)> callback);

  // Adds a pending window association for a URL.
  void AddPendingWindowAssociation(const GURL& url, const base::Uuid& task_id);

  // Gets and clears the pending window association for a URL.
  std::optional<base::Uuid> GetAndClearPendingWindowAssociation(
      const GURL& url);

  // Returns the URL for the default AI page. This is the URL that should be
  // loaded in the absence of any other context.
  virtual GURL GetDefaultAiPageUrl();

  // Returns the URL for the default AI page for a given task.
  virtual GURL GetDefaultAiPageUrlForTask(const base::Uuid& task_id);

  // Returns whether the provided WebContents is a tracked window for any task.
  virtual bool IsTrackedWindow(content::WebContents* web_contents);

  // Returns the RenderFrameHost that is tied to the `target_rfh` message proxy.
  // This is used to route messages from opened windows back to the <webview>
  // that opened them, even across separate StoragePartitions. If
  //  `target_rfh` isn't a tracked message proxy or `source_origin` isn't an
  //  allowlisted origin to send postMessages to the <webview>, then returns
  //  null.
  content::RenderFrameHost* GetGuestForMessage(
      content::RenderFrameHost* target_rfh,
      const url::Origin& source_origin);

  // either in a full tab or in the side panel. If |task_id| is invalid, the
  // UI is in a zero-state that is waiting for user to create a new task.
  virtual void OnTaskChanged(BrowserWindowInterface* browser_window_interface,
                             content::WebContents* web_contents,
                             const std::optional<base::Uuid>& old_task_id,
                             const std::optional<base::Uuid>& new_task_id,
                             bool is_shown_in_tab);

  // Called when the WebUI is ready.
  virtual void OnWebUIReady(BrowserWindowInterface* browser_window_interface,
                            const base::Uuid& task_id,
                            content::WebContents* web_contents);

  // Called when the WebUI controller is destroyed.
  virtual void OnWebUIDestroyed(
      BrowserWindowInterface* browser_window_interface,
      const std::optional<base::Uuid>& task_id);

  // Turns on smart tab sharing in the specified browser window's active WebUI.
  virtual void TurnOnSmartTabSharing(BrowserWindowInterface* browser);

  // Opens the contextual tasks side panel and creates a new task with the given
  // URL as its initial thread URL.
  virtual void StartTaskUiInSidePanel(
      BrowserWindowInterface* browser_window_interface,
      tabs::TabInterface* tab_interface,
      const GURL& url,
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
          session_handle);

  // Opens the contextual tasks side panel showing a ghost loader while waiting
  // for the initial thread URL to be provided for that task. This creates an
  // empty task. If the panel is already open for a task, this is a no-op.
  virtual void InitSidePanelWithGhostLoader(
      BrowserWindowInterface* browser_window_interface,
      tabs::TabInterface* tab_interface,
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

  // Gets the contextual task Id from a contextual task host URL.
  static base::Uuid GetTaskIdFromUrl(const GURL& url);

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

  // Returns a copy of base_url with the URL params from webui_url applied to
  // it. This will exclude chrome webui-specific params, specifically "task".
  static GURL CopyParamsFromWebUIUrl(const GURL& base_url,
                                     const GURL& webui_url);

  // Returns a copy of base_url with the URL params from webui_url applied to
  // it. If the result is empty, returns base_url.
  static GURL GetAiUrlFromWebUIUrl(const GURL& base_url, const GURL& webui_url);

  // Returns whether the provided host is trusted for overrides.
  static bool IsTrustedHost(const std::string& host);

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

  // Returns the eligibility manager.
  virtual ContextualTasksEligibilityManager* GetEligibilityManager() const;

  // Fetches an access token for the primary account.
  using GetAccessTokenCallback = base::OnceCallback<void(const std::string&)>;
  virtual void GetAccessToken(GetAccessTokenCallback callback,
                              base::WeakPtr<content::WebContents> web_contents);

  // Gets the entry point override for a given task.
  omnibox::ChromeAimEntryPoint GetInitialEntryPointForTask(
      const base::Uuid& task_id);

  // Show the feedback form.
  virtual void OpenFeedbackUi(BrowserWindowInterface* browser,
                              const GURL& page_url);

  // Shows a snackbar to undo the closure of the contextual tasks sheet.
  virtual void ShowUndoSnackbar(
      BrowserWindowInterface* browser_window_interface);

  // Returns whether the provided URL is for the primary account in Chrome.
  virtual bool IsUrlForPrimaryAccount(const GURL& url);

  const std::vector<std::unique_ptr<ContextualTasksWindowTracker>>&
  window_trackers_for_testing() const;

  base::WeakPtr<ContextualTasksUiService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  // The actual implementation of `HandleNavigation` that extracts more of the
  // components needed to decide if the navigation should be handled by this
  // service.
  virtual bool HandleNavigationImpl(
      content::OpenURLParams url_params,
      content::WebContents* source_contents,
      tabs::TabInterface* tab,
      bool is_from_embedded_page,
      bool from_can_create_window,
      bool is_same_site_or_from_ui,
      bool is_mobile_ua,
      const std::optional<url::Origin>& initiator_origin,
      const std::optional<content::GlobalRenderFrameHostToken>&
          initiator_frame_token);

  // Used primarily for debugging - loads a URL in the specified WebContents.
  virtual void LoadUrlInWebContents(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents);

  // Creates a LensMediaLinkHandler for the given WebContents.
  // Virtual to allow overriding in tests to mock the handler.
#if !BUILDFLAG(IS_ANDROID)
  virtual std::unique_ptr<lens::LensMediaLinkHandler> CreateMediaLinkHandler(
      content::WebContents* web_contents);
#endif

  // Handles PDF citation links by scrolling to page if applicable.
  // Returns true if handled.
  // Virtual to allow overriding in tests.
  virtual bool MaybeHandlePdfCitation(const GURL& url,
                                      tabs::TabInterface* tab,
                                      const base::Uuid& task_id);

 private:
  void StartAccessTokenFetch();

  // Creates a WebContents for the given origin to be used as an
  // opener for message routing. This WebContents is not loading any content.
  // Origin will be used as the initiator_origin and initiator_base_url for the
  // created WebContents. This is the value shown to window.opener.origin`
  // when assigned to an opener. This should be the same origin as the
  // initiator of creating the WebContents that this will be the proxy for.
  std::unique_ptr<content::WebContents> CreateMessageProxyWebContents(
      const url::Origin& origin);

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

  // Called when the contextual tasks eligibility changes.
  void OnEligibilityChanged(bool eligible);

  // Focus an existing tab based on the provided URL if it exists. The URLs are
  // compared without text selection directives as they don't change the page
  // content and only tell the browser what text to highlight on the page. A
  // pointer to the selected tab is returned if found.
  tabs::TabInterface* MaybeFocusExistingOpenTab(const GURL& url,
                                                TabListInterface* tab_list,
                                                const base::Uuid& task_id);

  // Handles video citation links by seeking existing video if applicable.
  // Returns true if handled.
  bool MaybeHandleVideoCitation(const GURL& url,
                                tabs::TabInterface* tab,
                                const base::Uuid& task_id);

#if !BUILDFLAG(IS_ANDROID)
  // Called when back button expands side panel.
  void OnBackButtonExpandsSidePanel(base::WeakPtr<tabs::TabInterface> weak_tab);
#endif

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
  virtual void OpenUrl(const content::OpenURLParams& url_params);

  // Sets the initial thread URL for a given task and runs any pending
  // callbacks.
  virtual void OnInitialThreadUrlAvailable(const base::Uuid& task_id,
                                           const GURL& url);

  // Checks if the provided URL matches any of the allowed hosts.
  static bool IsAllowedHost(const GURL& url);

  // Returns the host override for a given task if it differs from the default.
  std::string GetHostForTask(const base::Uuid& task_id);

  // Removes a window tracker from the list of trackers.
  void RemoveWindowTracker(base::WeakPtr<ContextualTasksWindowTracker> tracker);

 private:
  base::ObserverList<Observer> observers_;

  const raw_ptr<Profile> profile_;

  // The delegate to perform platform specific tasks.
  std::unique_ptr<ContextualTasksUiServiceDelegate> delegate_;

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

  // The cookie synchronizer for the isolated partition.
  std::unique_ptr<ContextualTasksCookieSynchronizer> cookie_synchronizer_;

  // Helper to manage Contextual Tasks eligibility.
  std::unique_ptr<ContextualTasksEligibilityManager> eligibility_manager_;
  base::CallbackListSubscription eligibility_subscription_;
  bool is_eligible_ = false;

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

  // Map of tasks that are waiting for a URL to be generated. The value is a
  // callback to be run when the URL becomes available, or null if no callback
  // has been added yet.
  std::map<base::Uuid, base::OnceCallback<void(const GURL&)>>
      tasks_waiting_for_url_;

  // Manager for window trackers. Class responsible for creating and destroying
  // trackers and matching them to URLs and WebContents.
  std::unique_ptr<ContextualTasksWindowTrackerManager> tracker_manager_;

  base::WeakPtrFactory<ContextualTasksUiService> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_H_
