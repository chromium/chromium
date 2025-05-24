// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_HOST_H_
#define CHROME_BROWSER_GLIC_HOST_HOST_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"

class Profile;
namespace content {
class WebContents;
class RenderProcessHost;
}  // namespace content
namespace glic {
class GlicKeyedService;
class GlicPageHandler;
class WebUIContentsContainer;

// The host owns the WebUI that contains the main glic UI and the web client.
// TODO(crbug.com/409332639): Better encapsulate details here.
class Host {
 public:
  class Delegate {
   public:
    // Returns the current panel state.
    virtual const mojom::PanelState& GetPanelState() const = 0;
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
  };

  explicit Host(Profile* profile);
  ~Host();

  void Initialize(Delegate* delegate);

  void PanelWillOpen(mojom::InvocationSource invocation_source);

  void PanelWasClosed();

  // Must be called before the delegate is destroyed, just before Host's
  // destructor.
  void Destroy();

  // Delete the owned web contents and prepare for destruction.
  void Shutdown();

  // Creates the web contents that will own the Glic WebUI.
  void CreateContents();

  // Called when a `GlicPageHandler` is created.
  void WebUIPageHandlerAdded(GlicPageHandler* page_handler);

  // Called when a `GlicPageHandler` is about to be destroyed.
  void WebUIPageHandlerRemoved(GlicPageHandler* page_handler);

  // Called when a login page was committed in a glic webview.
  void LoginPageCommitted(GlicPageHandler* page_handler);

  // Called when a glic guest (webview web contents) is added.
  void GuestAdded(content::WebContents* guest_contents);

  // Signals the glic WebUI that the glic window will be shown soon.
  void NotifyWindowIntentToShow();

  // Returns the page handler that owns the WebUI web contents.
  GlicPageHandler* FindPageHandlerForWebUiContents(
      const content::WebContents* webui_contents);

  // Called when a page handler's web client is created or destroyed.
  void SetWebClient(GlicPageHandler* page_handler,
                    GlicWebClientAccess* web_client);
  void WebClientInitializeFailed(GlicWebClientAccess* web_client);

  WebUIContentsContainer* contents_container() { return contents_.get(); }
  // Returns the WebUI web contents. May be null.
  content::WebContents* webui_contents();

  // Returns whether `contents` is the glic WebUI web contents.
  bool IsGlicWebUi(content::WebContents* contents);

  // Returns whether `host` is the glic WebUI render process host.
  bool IsGlicWebUiHost(content::RenderProcessHost* host);

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

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Informs the host that the WebUi state has changed.
  void WebUiStateChanged(GlicPageHandler* page_handler,
                         mojom::WebUiState new_state);
  // Returns the current WebUI state, or kUninitialized if there is no active
  // glic WebUI.
  const mojom::WebUiState& GetPrimaryWebUiState() const {
    return primary_webui_state_;
  }

 private:
  GlicKeyedService& glic_service();

  struct PageHandlerInfo {
    PageHandlerInfo();
    ~PageHandlerInfo();
    PageHandlerInfo(PageHandlerInfo&&);
    PageHandlerInfo& operator=(PageHandlerInfo&&);

    raw_ptr<GlicPageHandler> page_handler = nullptr;
    // True if the response to PanelWillOpen was received. Cleared when
    // PanelWasClosed() is called.
    bool open_complete = false;
    raw_ptr<GlicWebClientAccess> web_client = nullptr;
  };
  void PanelWillOpenComplete(GlicWebClientAccess* client,
                             mojom::OpenPanelInfoPtr open_info);
  PageHandlerInfo* FindInfo(GlicPageHandler* handler);
  PageHandlerInfo* FindInfoForClient(GlicWebClientAccess* client);
  PageHandlerInfo* FindInfoForWebUiContents(content::WebContents* web_contents);

  raw_ptr<Profile> profile_;
  // Null before `Initialize()` and after `Shutdown()`.
  raw_ptr<Delegate> delegate_;
  base::ObserverList<Observer> observers_;

  // The invocation source if the panel is open. nullopt while the panel is
  // closed.
  std::optional<mojom::InvocationSource> invocation_source_;
  mojom::WebUiState primary_webui_state_ = mojom::WebUiState::kUninitialized;

  // The set of live `GlicPageHandler`s.
  std::vector<PageHandlerInfo> page_handlers_;
  // Keep profile alive as long as the glic web contents. This object should be
  // destroyed when the profile needs to be destroyed.
  std::unique_ptr<WebUIContentsContainer> contents_;
  // The primary page handler. Glic supports only a single primary page handler,
  // a page handlers becomes the primary when it's created, if there exists no
  // other primary page handler.
  raw_ptr<GlicPageHandler> primary_page_handler_ = nullptr;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_HOST_H_
