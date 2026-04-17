// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

namespace glic {
class GlicKeyedService;

// Handles the Mojo requests coming from the Glic Internals WebUI.
class GlicInternalsPageHandler : public glic::mojom::InternalsPageHandler {
 public:
  GlicInternalsPageHandler(
      content::WebContents* webui_contents,
      mojo::PendingReceiver<glic::mojom::InternalsPageHandler> receiver);

  GlicInternalsPageHandler(const GlicInternalsPageHandler&) = delete;
  GlicInternalsPageHandler& operator=(const GlicInternalsPageHandler&) = delete;

  ~GlicInternalsPageHandler() override;

  // glic::mojom::InternalsPageHandler implementation.
  void GetInternalsDataPayload(
      GetInternalsDataPayloadCallback callback) override;

  void SetGuestUrlPresets(const GURL& autopush_url,
                          const GURL& staging_url,
                          const GURL& preprod_url,
                          const GURL& prod_url) override;

  void TriggerInvokeFromInternalsAction(
      mojom::TriggerInvokeFromInternalsOptionsPtr options,
      TriggerInvokeFromInternalsActionCallback callback) override;

  void SetWebContinuityOriginatingHostUrlPreset(
      const GURL& web_continuity_originating_host_url) override;

  void SetShowErrorAllowed(bool allowed) override;

 private:
  GlicKeyedService* GetGlicService();

  raw_ptr<content::WebContents> webui_contents_;
  raw_ptr<content::BrowserContext> browser_context_;
  mojo::Receiver<glic::mojom::InternalsPageHandler> receiver_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_INTERNALS_PAGE_HANDLER_H_
