// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/task_info_delegate.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class BrowserWindowInterface;
class GoogleServiceAuthError;

namespace content {
struct OpenURLParams;
class BrowserContext;
class WebContentsObserver;
}  // namespace content

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace contextual_tasks {
class ContextualTasksService;
class ContextualTasksSidePanelCoordinator;
class ContextualTasksUiService;
}  // namespace contextual_tasks

namespace tabs {
class TabInterface;
}  // namespace tabs

class ContextualTasksComposeboxHandler;
class ContextualTasksInternalsPageHandler;

class ContextualTasksPageHandler;

class ContextualTasksUI : public TaskInfoDelegate,
                          public TopChromeWebUIController,
                          public contextual_tasks::mojom::PageHandlerFactory,
                          public composebox::mojom::PageHandlerFactory,
                          public contextual_tasks_internals::mojom::
                              ContextualTasksInternalsPageHandlerFactory {
 public:
  // A WebContentsObserver used to observe navigations or URL changes in the
  // frame being hosted by this WebUI. Top-level navigations are ignored since
  // this class is only intended to listen to the embedded AI frame.
  class FrameNavObserver : public content::WebContentsObserver {
   public:
    explicit FrameNavObserver(
        content::WebContents* web_contents,
        contextual_tasks::ContextualTasksUiService* ui_service,
        contextual_tasks::ContextualTasksService* contextual_tasks_service,
        TaskInfoDelegate* task_info_delegate);
    ~FrameNavObserver() override = default;

    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;

   private:
    raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;
    raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;
    raw_ref<TaskInfoDelegate> task_info_delegate_;

    // Last committed URL used to check if URL changes.
    GURL last_committed_url_;
  };

  explicit ContextualTasksUI(content::WebUI* web_ui);
  ContextualTasksUI(const ContextualTasksUI&) = delete;
  ContextualTasksUI& operator=(const ContextualTasksUI&) = delete;
  ~ContextualTasksUI() override;

  // composebox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<composebox::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override;

  // contextual_tasks::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<contextual_tasks::mojom::Page> page,
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler)
      override;

  // TaskInfoDelegate impl:
  const std::optional<base::Uuid>& GetTaskId() override;
  void SetTaskId(std::optional<base::Uuid> id) override;
  const std::optional<std::string>& GetThreadId() override;
  void SetThreadId(std::optional<std::string> id) override;
  void SetThreadTurnId(std::optional<std::string> id) override;
  const std::optional<std::string>& GetThreadTitle() override;
  void SetThreadTitle(std::optional<std::string> title) override;
  void SetIsAiPage(bool is_ai_page) override;
  bool IsShownInTab() override;
  BrowserWindowInterface* GetBrowser() override;
  content::WebContents* GetWebUIWebContents() override;
  void OnZeroStateChange(bool is_zero_state) override;
  void OnTaskChanged() override;

  // Returns whether the given URL is an AI page zero state. This is used to
  // determine if the UI should be rendered in zero state. Static so it can be
  // used by the FrameNavObserver and easily tested.
  static bool IsZeroState(
      const GURL& url,
      contextual_tasks::ContextualTasksUiService* ui_service);

  // Get the URL of the page currently embedded in this WebUI.
  const GURL& GetInnerFrameUrl() const;

  void CloseSidePanel();

  // Lazily creates and returns a reference to the owned contextual search
  // session handle for `composebox_handler_`.
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();

  void BindInterface(
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the
  // composebox::mojom::PageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver);

  // Instantiates the implementor of the contextual_tasks::mojom::
  // ContextualTasksInternalsPageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<contextual_tasks_internals::mojom::
                                ContextualTasksInternalsPageHandlerFactory>
          pending_receiver);

  // contextual_tasks::mojom::ContextualTasksInternalsPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<
          contextual_tasks_internals::mojom::ContextualTasksInternalsPage> page,
      mojo::PendingReceiver<contextual_tasks_internals::mojom::
                                ContextualTasksInternalsPageHandler> receiver)
      override;

  static constexpr std::string_view GetWebUIName() { return "ContextualTasks"; }

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  // Notify the UI that the WebContents has moved to or from the side panel or
  // tab.
  void OnSidePanelStateChanged();

  // Called to disable active tab context suggestion on compose box.
  virtual void DisableActiveTabContextSuggestion();

  // Called when the active tab has been changed, either a new page is loaded or
  // a title change. This is only called when the of this class is rendered in
  // the side panel.
  void OnActiveTabContextStatusChanged();

  // Notify the UI that the Lens overlay has either started showing or is now
  // hidden.
  void OnLensOverlayStateChanged(bool is_showing);

  void SetComposeboxHandlerForTesting(
      std::unique_ptr<ContextualTasksComposeboxHandler> handler) {
    composebox_handler_ = std::move(handler);
  }

  // Notify the UI of the page context eligibility of the page.
  void OnPageContextEligibilityChecked(bool is_page_context_eligible);

  // Called by the browser process to send a message to the <webview>
  // guest. The WebUI is responsible for taking the 'message' (a serialized
  // lens.ClientToAimMessage protobuf) and using the <webview> postMessage API
  // to send it to the guest content.
  virtual void PostMessageToWebview(const lens::ClientToAimMessage& message);

  mojo::Remote<contextual_tasks::mojom::Page>& page() { return page_; }

  // Transfers an existing navigation to the page embedded in this WebUI. This
  // API will only accept navigations to the AI or search results pages.
  void TransferNavigationToEmbeddedPage(content::OpenURLParams params);

  // Returns whether the active tab context suggestion is showing.
  bool IsActiveTabContextSuggestionShowing() const;

 private:
  void RequestOAuthToken();
  void OnOAuthTokenReceived(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);
  // A an observer specifically to watch for the creation of the hosted remote
  // page. This is attached to the WebContents for the WebUI and notifies the
  // WebUI when an inner WebContents is created. The expectation is that there
  // is only ever one inner WebContents at a time.
  class InnerFrameCreationObvserver : public content::WebContentsObserver {
   public:
    explicit InnerFrameCreationObvserver(
        content::WebContents* web_contents,
        base::OnceCallback<void(content::WebContents*)> callback);
    ~InnerFrameCreationObvserver() override;

    void InnerWebContentsCreated(
        content::WebContents* inner_web_contents) override;

   private:
    base::OnceCallback<void(content::WebContents*)> callback_;
  };

  // A notification that the WebContents hosting the WebUI has created an inner
  // WebContents. In practice, this is the creation of the WebContents hosting
  // the embedded remote page.
  void OnInnerWebContentsCreated(content::WebContents* inner_contents);

  // Called when the contextual task context is returned by the service.
  void OnContextRetrievedForActiveTab(
      int32_t tab_id,
      const GURL& last_committed_url,
      std::unique_ptr<contextual_tasks::ContextualTaskContext> context);

  // Called to update the suggested tab chip on composebox.
  void UpdateSuggestedTabContext(tabs::TabInterface* tab);

  // Update the task's details in the WebUI.
  void PushTaskDetailsToPage();

  contextual_tasks::ContextualTasksSidePanelCoordinator*
  GetSidePanelCoordinator();

  // The OAuth token fetcher is used to fetch the OAuth token for the signed in
  // user. This is used to authenticate the user when making requests in the
  // embedded page.
  std::unique_ptr<signin::AccessTokenFetcher> oauth_token_fetcher_;

  // A timer used to refresh the OAuth token before it expires.
  base::OneShotTimer token_refresh_timer_;

  std::unique_ptr<ContextualTasksComposeboxHandler> composebox_handler_;
  raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;

  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;

  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_handler_factory_receiver_{this};

  mojo::Receiver<contextual_tasks::mojom::PageHandlerFactory>
      contextual_tasks_page_handler_factory_receiver_{this};

  std::unique_ptr<ContextualTasksPageHandler> page_handler_;
  mojo::Remote<composebox::mojom::Page> page_remote_;

  std::unique_ptr<InnerFrameCreationObvserver>
      inner_web_contents_creation_observer_;
  std::unique_ptr<FrameNavObserver> nav_observer_;

  // A handle to the embedded page for this WebUI. This is the WebContents that
  // contains the AI thread (and sometimes the search results page).
  base::WeakPtr<content::WebContents> embedded_web_contents_;

  // The ID of the task (concept that owns one or more threads) associated with
  // this WebUI, if it exists. This is a cached value tied to the most recent
  // information we received from observing URL changes on the embedded page.
  // This will be empty for new threads (see below) or when loading a thread
  // that doesn't already have a task. If this value is changing, it is very
  // likely that `thread_id` should also change.
  std::optional<base::Uuid> task_id_;

  // The ID of the thread (concept representing a single session with an AI)
  // associated with this WebUI, if it exists. This will be empty for a new
  // thread and is used to detect changes in the embedded page. If this value is
  // changing, it is very likely that `task_id` should also change.
  std::optional<std::string> thread_id_;

  // The ID of the current turn (a single submission and response) for the
  // active thread, if it exists. This will be empty for a new thread and is
  // used to keep the UI URL up to date.
  std::optional<std::string> thread_turn_id_;

  std::optional<std::string> thread_title_;

  mojo::Remote<contextual_tasks::mojom::Page> page_;

  mojo::Receiver<contextual_tasks_internals::mojom::
                     ContextualTasksInternalsPageHandlerFactory>
      contextual_tasks_internals_page_handler_receiver_{this};

  std::unique_ptr<ContextualTasksInternalsPageHandler>
      contextual_tasks_internals_page_handler_;

  enum class WebUIState {
    kUnknown,
    kShownInTab,
    kShownInSidePanel,
  };
  WebUIState previous_web_ui_state_ = WebUIState::kUnknown;
  bool was_ai_page_ = false;

  base::WeakPtrFactory<ContextualTasksUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class ContextualTasksUIConfig
    : public DefaultTopChromeWebUIConfig<ContextualTasksUI> {
 public:
  ContextualTasksUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUIContextualTasksHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_H_
