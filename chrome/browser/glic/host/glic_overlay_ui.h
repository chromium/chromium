// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_OVERLAY_UI_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_OVERLAY_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic_overlay.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace glic {

// WebUI controller for the Glic overlay surface (`chrome://glic/overlay`).
//
// In NoWebview mode (`features::kGlicNoWebview`), Glic loads this lightweight
// WebUI overlay instead of embedding a webview in `GlicUI`. It is responsible
// for displaying loading skeletons and local error/offline panels while the
// guest page (`gemini.google.com/glic`) runs in a separate
// PrivilegedWebContents.
class GlicOverlayUI : public ui::MojoWebUIController,
                      public mojom::GlicOverlayPageHandlerFactory {
 public:
  explicit GlicOverlayUI(content::WebUI* web_ui);
  GlicOverlayUI(const GlicOverlayUI&) = delete;
  GlicOverlayUI& operator=(const GlicOverlayUI&) = delete;
  ~GlicOverlayUI() override;

  void SetPageHandler(mojom::GlicOverlayPageHandler* page_handler);

  void SetOverlayState(mojom::OverlayStatePtr state);

  void BindInterface(
      mojo::PendingReceiver<mojom::GlicOverlayPageHandlerFactory> receiver);

  // mojom::GlicOverlayPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::GlicOverlayPage> page,
      mojo::PendingReceiver<mojom::GlicOverlayPageHandler> receiver) override;

 private:
  raw_ptr<mojom::GlicOverlayPageHandler> page_handler_ = nullptr;
  mojo::Receiver<mojom::GlicOverlayPageHandlerFactory> page_factory_receiver_{
      this};
  std::optional<mojo::Receiver<mojom::GlicOverlayPageHandler>>
      page_handler_receiver_;
  mojo::Remote<mojom::GlicOverlayPage> page_remote_;
  mojom::OverlayStatePtr current_state_;
  mojo::PendingReceiver<mojom::GlicOverlayPageHandler> pending_receiver_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_OVERLAY_UI_H_
