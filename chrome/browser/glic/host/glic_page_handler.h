// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_PAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_PAGE_HANDLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content
namespace gfx {
class Size;
}  // namespace gfx

namespace glic {
class GlicKeyedService;
class GlicWebClientHandler;

// Handles the Mojo requests coming from the Glic WebUI.
class GlicPageHandler : public glic::mojom::PageHandler,
                        public PanelStateObserver {
 public:
  GlicPageHandler(content::WebContents* webui_contents,
                  mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
                  mojo::PendingRemote<glic::mojom::Page> page);

  GlicPageHandler(const GlicPageHandler&) = delete;
  GlicPageHandler& operator=(const GlicPageHandler&) = delete;

  ~GlicPageHandler() override;

  content::WebContents* webui_contents() { return webui_contents_; }

  void NotifyWindowIntentToShow();

  // Returns the main frame of the guest view that lives within this WebUI. May
  // be null.
  content::RenderFrameHost* GetGuestMainFrame();

  // glic::mojom::PageHandler implementation.

  void CreateWebClient(::mojo::PendingReceiver<glic::mojom::WebClientHandler>
                           web_client_receiver) override;
  void PrepareForClient(base::OnceCallback<void(mojom::PrepareForClientResult)>
                            callback) override;
  // Called whenever the webview main frame commits.
  void WebviewCommitted(const GURL& origin) override;

  void ClosePanel(ClosePanelCallback callback) override;

  void OpenProfilePickerAndClosePanel() override;

  void SignInAndClosePanel() override;

  void OpenDisabledByAdminLinkAndClosePanel() override;

  void ResizeWidget(const gfx::Size& size,
                    base::TimeDelta duration,
                    ResizeWidgetCallback callback) override;

  void EnableDragResize(bool enabled) override;

  // Notifies the web client about zero state suggestions.
  void ZeroStateSuggestionChanged(mojom::ZeroStateSuggestionsV2Ptr suggestions,
                                  mojom::ZeroStateSuggestionsOptions options);

  void WebUiStateChanged(glic::mojom::WebUiState new_state) override;

  // PanelStateObserver implementation.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state,
                         const PanelStateContext& context) override;

  void UpdatePageState(mojom::PanelStateKind panelStateKind);

  Host& host() { return *host_; }

 private:
  void UpdateProfileReadyState();

  GlicKeyedService* GetGlicService();

  // HostManager keeps the host alive while GlicPageHandler is alive.
  raw_ptr<Host> host_;
  // There should at most one WebClientHandler at a time. A new one is created
  // each time the webview loads a page.
  std::unique_ptr<GlicWebClientHandler> web_client_handler_;
  raw_ptr<content::WebContents> webui_contents_;
  raw_ptr<content::BrowserContext> browser_context_;
  mojo::Receiver<glic::mojom::PageHandler> receiver_;
  mojo::Remote<glic::mojom::Page> page_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  std::vector<base::CallbackListSubscription> subscriptions_;
  base::WeakPtrFactory<GlicPageHandler> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_PAGE_HANDLER_H_
