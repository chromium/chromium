// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_GLIC_PAGE_HANDLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
class WebContents;
}
namespace gfx {
class Size;
}  // namespace gfx

namespace glic {
class GlicKeyedService;
class GlicWebClientHandler;

// Handles the Mojo requests coming from the Glic WebUI.
class GlicPageHandler : public glic::mojom::PageHandler {
 public:
  GlicPageHandler(content::WebContents* webui_contents,
                  mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
                  mojo::PendingRemote<glic::mojom::Page> page);

  GlicPageHandler(const GlicPageHandler&) = delete;
  GlicPageHandler& operator=(const GlicPageHandler&) = delete;

  ~GlicPageHandler() override;

  content::WebContents* webui_contents() { return webui_contents_; }

  // Called whenever a guest view is added to the WebContents owned by this
  // WebUI. Because we embed only one webview, there should only ever be one
  // guest view alive at one time within GlicPageHandler.
  void GuestAdded(content::WebContents* guest_contents);

  void NotifyWindowIntentToShow();

  // Returns the guest view's WebContents that lives within this WebUI. May be
  // null.
  content::WebContents* guest_contents() { return guest_contents_.get(); }

  // glic::mojom::PageHandler implementation.

  void CreateWebClient(::mojo::PendingReceiver<glic::mojom::WebClientHandler>
                           web_client_receiver) override;
  void PrepareForClient(base::OnceCallback<void(bool)> callback) override;
  // Called whenever the webview main frame commits.
  void WebviewCommitted(const GURL& origin) override;

  void ClosePanel() override;

  void ResizeWidget(const gfx::Size& size,
                    base::TimeDelta duration,
                    ResizeWidgetCallback callback) override;

  void WebUiStateChanged(glic::mojom::WebUiState new_state) override;

 private:
  void EnableChange();
  GlicKeyedService* GetGlicService();

  // There should at most one WebClientHandler at a time. A new one is created
  // each time the webview loads a page.
  std::unique_ptr<GlicWebClientHandler> web_client_handler_;
  raw_ptr<content::WebContents> webui_contents_;
  raw_ptr<content::BrowserContext> browser_context_;
  base::WeakPtr<content::WebContents> guest_contents_;
  mojo::Receiver<glic::mojom::PageHandler> receiver_;
  mojo::Remote<glic::mojom::Page> page_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  std::vector<base::CallbackListSubscription> subscriptions_;
  base::WeakPtrFactory<GlicPageHandler> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_PAGE_HANDLER_H_
