// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_HOST_H_
#define CHROME_BROWSER_GLIC_HOST_HOST_H_

#include <deque>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_provider.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host_metrics.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/common/actor/task_id.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/widget/widget.h"

namespace actor {
class ActorTaskDelegate;
}  // namespace actor

class Profile;
namespace content {
class WebContents;
class RenderProcessHost;
}  // namespace content
namespace glic {
class GlicKeyedService;
class GlicPageHandler;
class GlicWindowController;
class WebUIContentsContainer;
class GlicInstanceMetrics;

// The host owns the WebUI that contains the main glic UI and the web client.
// TODO(crbug.com/409332639): Better encapsulate details here.
class Host : public GlicSharingManagerProvider {
 public:
  class EmbedderDelegate {
   public:
    virtual ~EmbedderDelegate() = default;

    // Sets the size of the glic window to the specified dimensions. Callback
    // runs when the animation finishes or is destroyed, or soon if the window
    // doesn't exist yet. In this last case `size` will be used for the
    // initial size when creating the widget later.
    virtual void Resize(const gfx::Size& size,
                        base::TimeDelta duration,
                        base::OnceClosure callback) = 0;
    // Sets the areas of the view from which it should be draggable.
    virtual void SetDraggableAreas(
        const std::vector<gfx::Rect>& draggable_areas) = 0;
    // Allows the user to manually resize the widget by dragging. If the widget
    // hasn't been created yet, apply this setting when it is created. No effect
    // if the widget doesn't exist or the feature flag is disabled.
    virtual void EnableDragResize(bool enabled) = 0;

    // Attaches glic to the last focused Chrome window.
    virtual void Attach() = 0;
    virtual void Detach() = 0;
    virtual void ClosePanel() = 0;
    // Sets the minimum widget size that the widget will allow the user to
    // resize
    // to.
    virtual void SetMinimumWidgetSize(const gfx::Size& size) = 0;
    virtual void CaptureScreenshot(
        glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) = 0;
    // Returns true if the glic widget is visible.
    virtual bool IsShowing() const = 0;

    virtual void SwitchConversation(
        glic::mojom::ConversationInfoPtr info,
        mojom::WebClientHandler::SwitchConversationCallback callback) = 0;
  };

  // Functions that are on either GlicInstance or GlidKeyedService.
  // Interface for methods that the host can call on an instance.
  // TODO(refactor): This interface should eventually be combined with
  // InstanceInterfaceForMigration.
  class InstanceDelegate {
   public:
    virtual ~InstanceDelegate() = default;
    virtual tabs::TabInterface* CreateTab(
        const ::GURL& url,
        bool open_in_background,
        const std::optional<int32_t>& window_id,
        glic::mojom::WebClientHandler::CreateTabCallback callback) = 0;
    // TODO(mcnee): `delegate` appears unused.
    virtual void CreateTask(
        base::WeakPtr<actor::ActorTaskDelegate> delegate,
        actor::webui::mojom::TaskOptionsPtr options,
        mojom::WebClientHandler::CreateTaskCallback callback) = 0;
    virtual void PerformActions(
        const std::vector<uint8_t>& actions_proto,
        mojom::WebClientHandler::PerformActionsCallback callback) = 0;
    virtual void StopActorTask(actor::TaskId task_id,
                               mojom::ActorTaskStopReason stop_reason) = 0;
    virtual void PauseActorTask(actor::TaskId task_id,
                                mojom::ActorTaskPauseReason pause_reason,
                                tabs::TabInterface::Handle tab_handle) = 0;
    virtual void ResumeActorTask(
        actor::TaskId task_id,
        const mojom::GetTabContextOptions& context_options,
        glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) = 0;
    virtual void InterruptActorTask(actor::TaskId task_id) = 0;
    virtual void UninterruptActorTask(actor::TaskId task_id) = 0;

    virtual void CreateActorTab(
        actor::TaskId task_id,
        bool open_in_background,
        const std::optional<int32_t>& initiator_tab_id,
        const std::optional<int32_t>& initiator_window_id,
        glic::mojom::WebClientHandler::CreateActorTabCallback callback) = 0;

    virtual void FetchZeroStateSuggestions(
        bool is_first_run,
        std::optional<std::vector<std::string>> supported_tools,
        glic::mojom::WebClientHandler::
            GetZeroStateSuggestionsForFocusedTabCallback callback) = 0;

    virtual void GetZeroStateSuggestionsAndSubscribe(
        bool has_active_subscription,
        const mojom::ZeroStateSuggestionsOptions& options,
        mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
            callback) = 0;

    virtual void RegisterConversation(
        glic::mojom::ConversationInfoPtr info,
        mojom::WebClientHandler::RegisterConversationCallback callback) = 0;

    virtual void OnWebClientCleared() = 0;
    virtual void PrepareForOpen() = 0;

    virtual void OnInteractionModeChange(mojom::WebClientMode new_mode) = 0;
    virtual GlicInstanceMetrics* instance_metrics() = 0;

    virtual bool IsActive() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the client is ready to show, invoked sometime after
    // `Host::PanelWillOpen()` is called.
    virtual void ClientReadyToShow(const mojom::OpenPanelInfo&) {}

    // TODO(b/409332639): These signals are dubious, is this what window
    // controller really wants to know?

    // Called when the web client initialize has failed.
    virtual void WebClientInitializeFailed() {}
    // The webview reached a login page.
    virtual void LoginPageCommitted() {}
    // Called when the WebUI state changes in the glic WebUI.
    // If the glic WebUI is destroyed, the webUI state is returned to
    // kUninitialized.
    virtual void WebUiStateChanged(mojom::WebUiState state) {}
    // Called when the current view changes in the glic WebUI.
    virtual void OnViewChanged(mojom::CurrentView view) {}
    virtual void ContextAccessIndicatorChanged(bool enabled) {}
  };

  // When no sharing manager provider is supplied, GlicKeyedService is used.
  explicit Host(Profile* profile,
                GlicSharingManagerProvider* sharing_manager_provider,
                GlicInstance* glic_instance,
                InstanceDelegate* instance_delegate);
  Host(const Host&) = delete;
  ~Host() override;
  Host& operator=(const Host&) = delete;

  void SetDelegate(EmbedderDelegate* delegate);

  struct PanelWillOpenOptions {
    PanelWillOpenOptions();
    ~PanelWillOpenOptions();
    PanelWillOpenOptions(PanelWillOpenOptions&&);
    PanelWillOpenOptions& operator=(PanelWillOpenOptions&&);

    // The ID of the conversation to open. If unset, the web client will open a
    // new conversation.
    std::optional<std::string> conversation_id;
    // If set, the textbox for user input will be populated with the given
    // string before the panel opens.
    std::optional<std::string> prompt_suggestion;
    // Up to 3 most recently active conversations, ordered by most recently
    // active first.
    std::optional<std::vector<glic::mojom::ConversationInfoPtr>>
        recently_active_conversations;
  };
  void PanelWillOpen(mojom::InvocationSource invocation_source,
                     PanelWillOpenOptions options);

  void PanelWasClosed();

  void SwitchConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback);

  void RegisterConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::RegisterConversationCallback callback);

  // Delete the owned web contents and prepare for destruction.
  void Shutdown();

  // Request panel closing.
  void Close();
  // Reload the web contents.
  void Reload();

  // Creates the web contents that will own the Glic WebUI.
  // `initially_hidden` value is only relevant when
  // `kGlicGuestContentsVisibilityState` flag is enabled, otherwise the default
  // value is used (i.e. false).
  void CreateContents(bool initially_hidden);

  // Signals the glic WebUI that the glic window will be shown soon.
  void NotifyWindowIntentToShow();

  // GlicSharingManagerProvider Implementation.
  GlicSharingManager& sharing_manager() override;

  Host::InstanceDelegate& instance_delegate();

  GlicInstanceMetrics* instance_metrics() {
    return instance_delegate().instance_metrics();
  }

  WebUIContentsContainer* contents_container() { return contents_.get(); }
  // Returns the WebUI web contents. May be null.
  content::WebContents* webui_contents() const;

  // Returns the WebClient web contents. May be null.
  content::WebContents* web_client_contents() const;

  // Returns whether `contents` is the glic WebUI web contents.
  bool IsGlicWebUi(content::WebContents* contents) const;

  // Returns the list of page handlers for glic WebUI pages.
  std::vector<GlicPageHandler*> GetPageHandlersForTesting();
  GlicPageHandler* GetPrimaryPageHandlerForTesting();

  // TODO(b/409332639): Hide direct access to the web client.
  GlicWebClientAccess* GetPrimaryWebClient();

  // Whether the primary client is alive and has returned from PanelWillOpen().
  // This transitions to false after PanelWasClosed() is called.
  bool IsPrimaryClientOpen();

  // Whether the primary web client is connected.
  bool IsReady() const;
  bool IsContextAccessIndicatorEnabled() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void AddPanelStateObserver(PanelStateObserver* observer);
  void RemovePanelStateObserver(PanelStateObserver* observer);

  // Returns the current WebUI state, or kUninitialized if there is no active
  // glic WebUI.
  const mojom::WebUiState& GetPrimaryWebUiState() const {
    return primary_webui_state_;
  }

  // Informs the host that the Zero State Suggestions have changed.
  void NotifyZeroStateSuggestion(mojom::ZeroStateSuggestionsV2Ptr suggestions,
                                 mojom::ZeroStateSuggestionsOptions options);

  // Sends a ViewChangeRequest to the primary client.
  void SendViewChangeRequest(mojom::ViewChangeRequestPtr change_request);

  void NotifyInstanceActivationChanged(bool is_active);

  // Informs the web client that additional context is available.
  void NotifyAdditionalContext(mojom::AdditionalContextPtr context);

  // Returns the current view (conversation or actuation) in the floaty.
  mojom::CurrentView GetPrimaryCurrentView();

  // Returns the page handler that owns the WebUI web contents.
  GlicPageHandler* FindPageHandlerForWebUiContents(
      const content::WebContents* webui_contents);

  // Called when a glic guest (webview web contents) is added.
  void GuestAdded(content::WebContents* guest_contents);

  //////////////////////////////////////////////////////////////////////////
  // Methods intended to be used by page handler or web client handler
  //////////////////////////////////////////////////////////////////////////

  // Called when a login page was committed in a glic webview.
  void LoginPageCommitted(GlicPageHandler* page_handler);

  // Called when a page handler's web client is created or destroyed.
  void SetWebClient(GlicWebClientAccess* web_client);
  void UnsetWebClient(GlicWebClientAccess* web_client);
  void WebClientInitializeFailed(GlicWebClientAccess* web_client);

  void SetContextAccessIndicator(GlicPageHandler*, bool enabled);

  // Informs the host that the WebUi state has changed.
  void WebUiStateChanged(GlicPageHandler* page_handler,
                         mojom::WebUiState new_state);

  // Called when the current view changes in the glic webUI to update the state.
  void OnViewChanged(GlicWebClientAccess* client, mojom::CurrentView new_view);

  // Called when the web client changes its mode.
  void OnInteractionModeChange(GlicPageHandler* page_handler,
                               mojom::WebClientMode new_mode);

  // Sets the size of the glic window to the specified dimensions. Callback
  // runs when the animation finishes or is destroyed, or soon if the window
  // doesn't exist yet. In this last case `size` will be used for the
  // initial size when creating the widget later.
  void ResizePanel(GlicPageHandler* page_handler,
                   const gfx::Size& size,
                   base::TimeDelta duration,
                   base::OnceClosure callback);

  // Allows the user to manually resize the widget by dragging. If the widget
  // hasn't been created yet, apply this setting when it is created. No effect
  // if the widget doesn't exist or the feature flag is disabled.
  void EnableDragResize(GlicPageHandler* page_handler, bool enabled);

  void AttachPanel(GlicPageHandler* page_handler);
  void DetachPanel(GlicPageHandler* page_handler);
  void ClosePanel(GlicPageHandler* page_handler);
  // Sets the areas of the view from which it should be draggable.
  void SetPanelDraggableAreas(GlicPageHandler* page_handler,
                              const std::vector<gfx::Rect>& draggable_areas);

  // Sets the minimum widget size that the widget will allow the user to resize
  // to.
  void SetMinimumWidgetSize(GlicPageHandler* page_handler,
                            const gfx::Size& size);

  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback);

  // Returns true if the widget is visible.
  bool IsWidgetShowing(GlicWebClientAccess* client) const;
  // Returns the current panel state.
  mojom::PanelState GetPanelState(GlicWebClientAccess* client) const;

  base::WeakPtr<Host> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  void RequestToShowCredentialSelectionDialog(
      actor::TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      actor::ActorTaskDelegate::CredentialSelectedCallback callback);

  void RequestToShowUserConfirmationDialog(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_blocklisted_origin,
      actor::ActorTaskDelegate::UserConfirmationDialogCallback callback);

  void RequestToConfirmNavigation(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      actor::ActorTaskDelegate::NavigationConfirmationCallback callback);

  void RequestToShowAutofillSuggestionsDialog(
      actor::TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      actor::ActorTaskDelegate::AutofillSuggestionSelectedCallback callback);

  void FloatingPanelCanAttachChanged(bool can_attach);

  // Returns if the outer frame matches either the WebUI frame or the guest
  // frame.
  bool IsWebContentPresentAndMatches(content::RenderFrameHost* rfh);

 private:
  friend class HostManager;

  void WebUIPageHandlerAdded(GlicPageHandler* page_handler);
  void WebUIPageHandlerRemoved(GlicPageHandler* page_handler);
  GlicKeyedService& glic_service();
  GlicPageHandler* page_handler() const;
  bool IsGlicWebUiHost(content::RenderProcessHost* host) const;

  // Information about the page handler which is cleared when the page handler
  // goes away.
  struct PageHandlerInfo {
    PageHandlerInfo();
    ~PageHandlerInfo();
    PageHandlerInfo(PageHandlerInfo&&);
    PageHandlerInfo& operator=(PageHandlerInfo&&);

    raw_ptr<GlicPageHandler> page_handler = nullptr;
    // True if the response to PanelWillOpen was received. Cleared when
    // PanelWasClosed() is called.
    bool open_complete = false;
    bool context_access_indicator_enabled = false;
    raw_ptr<GlicWebClientAccess> web_client = nullptr;
  };

  void PanelWillOpenComplete(GlicWebClientAccess* client,
                             mojom::OpenPanelInfoPtr open_info);
  PageHandlerInfo* FindInfo(GlicPageHandler* handler);
  const PageHandlerInfo* FindInfo(GlicPageHandler* handler) const {
    return const_cast<Host*>(this)->FindInfo(handler);
  }
  PageHandlerInfo* FindInfoForClient(GlicWebClientAccess* client);
  PageHandlerInfo* FindInfoForWebUiContents(content::WebContents* web_contents);
  const PageHandlerInfo* FindInfoForWebUiContents(
      content::WebContents* web_contents) const {
    return const_cast<Host*>(this)->FindInfoForWebUiContents(web_contents);
  }

  raw_ptr<Profile> profile_;

  // The instance that owns this host.
  raw_ptr<InstanceDelegate> instance_delegate_;
  // May be null for hosts which are bound to chrome://glic tabs.
  raw_ptr<GlicInstance> glic_instance_;

  // Null before `Initialize()` and after `Shutdown()`.
  raw_ptr<EmbedderDelegate> delegate_;
  base::ObserverList<Observer> observers_;

  // The invocation source if the panel is open. nullopt while the panel is
  // closed.
  std::optional<mojom::InvocationSource> invocation_source_;
  std::optional<PanelWillOpenOptions> pending_panel_open_options_;
  mojom::WebUiState primary_webui_state_ = mojom::WebUiState::kUninitialized;
  std::optional<mojom::PanelState> pending_panel_state_;

  // Owns the WebUI contents. May be null for glic hosts in chrome://glic tabs.
  // Keep profile alive as long as the glic web contents. This object should be
  // destroyed when the profile needs to be destroyed.
  std::unique_ptr<WebUIContentsContainer> contents_;
  std::optional<PageHandlerInfo> handler_info_;

  raw_ptr<GlicSharingManagerProvider> sharing_manager_provider_;

  // The current view in the primary page handler.
  mojom::CurrentView primary_current_view_ = mojom::CurrentView::kConversation;

  base::WeakPtr<content::WebContents> web_client_contents_;

  HostMetrics metrics_;

  base::WeakPtrFactory<Host> weak_ptr_factory_{this};
};

// A Host::Delegate which does nothing. For chrome://glic tabs or inactive
// embedders.
class EmptyEmbedderDelegate : public Host::EmbedderDelegate {
 public:
  ~EmptyEmbedderDelegate() override = default;
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void SetDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas) override {}
  void EnableDragResize(bool enabled) override {}
  void Attach() override {}
  void Detach() override {}
  void ClosePanel() override {}
  void SetMinimumWidgetSize(const gfx::Size& size) override {}
  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback)
      override;
  bool IsShowing() const override;
  void SwitchConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;

 private:
  mojom::PanelState panel_state_ =
      mojom::PanelState(mojom::PanelStateKind::kDetached, std::nullopt);
};

// Manages hosts. Note, this is a stopgap that will be replaced by something
// else soon.
class HostManager {
 public:
  HostManager(Profile* profile,
              base::WeakPtr<GlicWindowController> window_controller);
  ~HostManager();

  void Shutdown();

  // Called when a `GlicPageHandler` is created.
  Host* WebUIPageHandlerAdded(GlicPageHandler* page_handler);
  // Called when a `GlicPageHandler` is about to be destroyed.
  void WebUIPageHandlerRemoved(GlicPageHandler* page_handler);

  // Called when a glic guest (webview web contents) is added.
  void GuestAdded(content::WebContents* guest_contents);

  // Returns whether `host` is the glic WebUI render process host.
  bool IsGlicWebUiHost(content::RenderProcessHost* host);

  // Returns whether `contents` is the glic WebUI web contents.
  bool IsGlicWebUi(content::WebContents* contents);

  // Get pointers to all Hosts, including those for chrome://glic in a tab.
  std::vector<Host*> GetAllHosts();

  Host* FindHostForTabForTesting(tabs::TabInterface& tab);

 private:
  std::vector<Host*> GetPrimaryHosts();
  raw_ptr<Profile> profile_;
  base::WeakPtr<GlicWindowController> window_controller_;
  std::unique_ptr<EmptyEmbedderDelegate> empty_embedder_delegate_;
  // Hosts for any unclaimed page handlers, which is approximately limited to
  // chrome://glic in tabs. These are only important for developers, and do not
  // need to be fully functional.
  std::vector<std::unique_ptr<Host>> tab_hosts_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_HOST_H_
