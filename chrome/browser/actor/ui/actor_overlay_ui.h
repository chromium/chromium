// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_UI_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_UI_H_

#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace actor::ui {
class ActorOverlayHandler;

class ActorOverlayUI : public ::ui::MojoWebUIController,
                       public mojom::ActorOverlayPageHandlerFactory {
 public:
  explicit ActorOverlayUI(content::WebUI* web_ui);
  ActorOverlayUI(const ActorOverlayUI&) = delete;
  ActorOverlayUI& operator=(const ActorOverlayUI&) = delete;

  ~ActorOverlayUI() override;

  // Instantiates the implementor of the
  // actor::ui::mojom::ActorOverlayPageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::ActorOverlayPageHandlerFactory> receiver);

  void SetOverlayBackground(bool is_visible);
  void SetBorderGlowVisibility(bool is_visible);

  // Checks if the passed in WebContents are associated with the ActorOverlayUI
  // WebUIController.
  static bool IsActorOverlayWebContents(content::WebContents* web_contents);

 private:
  // The PendingRemote must be valid and bind to a receiver in order to start
  // sending messages to the receiver.
  void CreatePageHandler(
      mojo::PendingRemote<mojom::ActorOverlayPage> page,
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) override;

  mojo::Receiver<mojom::ActorOverlayPageHandlerFactory> page_factory_receiver_{
      this};

  std::unique_ptr<ActorOverlayHandler> handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class ActorOverlayUIConfig
    : public content::DefaultWebUIConfig<ActorOverlayUI> {
 public:
  ActorOverlayUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIActorOverlayHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_UI_H_
