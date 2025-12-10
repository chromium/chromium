// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_UI_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace glic {
class GlicPreloadHandler;
class GlicPageHandler;
class GlicFrePageHandler;
class GlicUI;
class Host;

class GlicUIConfig : public content::DefaultWebUIConfig<GlicUI> {
 public:
  GlicUIConfig();
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://glic
class GlicUI : public ui::MojoWebUIController,
               public glic::mojom::PageHandlerFactory,
               public glic::mojom::FrePageHandlerFactory,
               public glic::mojom::GlicPreloadHandlerFactory {
 public:
  explicit GlicUI(content::WebUI* web_ui);
  ~GlicUI() override;

  // Returns the GlicUI controller for the given WebContents, or nullptr if it
  // doesn't exist or is not a GlicUI.
  static GlicUI* From(content::WebContents* web_contents);

  void BindInterface(
      mojo::PendingReceiver<glic::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<glic::mojom::FrePageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<glic::mojom::GlicPreloadHandlerFactory> receiver);

  // When called, the UI will believe it is offline when it is launched from the
  // current test.
  static void simulate_no_connection_for_testing() {
    simulate_no_connection_ = true;
  }

  // Associates the WebUI with a given Host. This must be called exactly once.
  void AttachToHost(Host* host);

 private:
  void CreatePageHandler(
      mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
      mojo::PendingRemote<glic::mojom::Page> page) override;

  void CreatePreloadHandler(
      mojo::PendingReceiver<glic::mojom::GlicPreloadHandler> receiver,
      mojo::PendingRemote<glic::mojom::PreloadPage> page) override;

  void CreatePageHandler(
      mojo::PendingReceiver<glic::mojom::FrePageHandler> fre_receiver) override;

  std::unique_ptr<GlicPreloadHandler> preload_handler_;
  std::unique_ptr<GlicPageHandler> page_handler_;
  std::unique_ptr<GlicFrePageHandler> fre_page_handler_;

  mojo::Receiver<glic::mojom::PageHandlerFactory> page_factory_receiver_{this};
  mojo::Receiver<glic::mojom::FrePageHandlerFactory> fre_page_factory_receiver_{
      this};
  mojo::Receiver<glic::mojom::GlicPreloadHandlerFactory>
      preload_factory_receiver_{this};

  // Raw pointer to the host this UI is attached to. This object is not owned
  // by GlicUI. Its lifetime is managed by GlicKeyedService (single-instance) or
  // GlicInstanceImpl (multi-instance).
  //
  // In the single-instance path, `HostManager` (owned by `GlicKeyedService`)
  // owns `Host`s. `HostManager::Shutdown()` is called during
  // `GlicKeyedService::Shutdown()`, which destroys all hosts and thus their
  // associated WebUIs.
  //
  // In the multi-instance path, `GlicInstanceImpl` owns `Host`. The
  // `GlicInstanceImpl` calls `Shutdown()` on the `Host` in its destructor,
  // which destroys the WebUI (and thus this `GlicUI`), ensuring `host_`
  // outlives `this`.
  raw_ptr<Host> host_ = nullptr;

  mojo::PendingReceiver<glic::mojom::PageHandler> pending_receiver_;
  mojo::PendingRemote<glic::mojom::Page> pending_page_;

  static bool simulate_no_connection_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_UI_H_
