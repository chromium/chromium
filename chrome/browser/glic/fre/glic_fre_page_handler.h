// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_GLIC_FRE_PAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_FRE_GLIC_FRE_PAGE_HANDLER_H_

#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;
namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace glic {
class GlicKeyedService;

// Handles the Mojo requests coming from the Glic FRE WebUI.
class GlicFrePageHandler : public glic::mojom::FrePageHandler {
 public:
  GlicFrePageHandler(
      content::WebContents* webui_contents,
      mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver);

  GlicFrePageHandler(const GlicFrePageHandler&) = delete;
  GlicFrePageHandler& operator=(const GlicFrePageHandler&) = delete;

  ~GlicFrePageHandler() override;

  // glic::mojom::FrePageHandler implementation.
  void AcceptFre() override;
  void DismissFre() override;
  void PrepareForClient(base::OnceCallback<void(bool)> callback) override;
  void ValidateAndOpenLinkInNewTab(const GURL& url) override;
  void WebUiStateChanged(mojom::FreWebUiState new_state) override;

 private:
  content::BrowserContext* browser_context() const;
  GlicKeyedService* GetGlicService();

  // Safe because `this` is owned by GlicUI` ,which is a `MojoWebUIController`
  // and won't outlive its `WebContents`.
  const raw_ptr<content::WebContents> webui_contents_;
  mojo::Receiver<glic::mojom::FrePageHandler> receiver_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_GLIC_FRE_PAGE_HANDLER_H_
