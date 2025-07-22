// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_

#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace actor::ui {

// Manages the browser-side logic for the chrome://actor-overlay WebUI. This
// controller implements mojom::ActorOverlayPageHandler to receive events from
// the renderer and delegates them to the ActorUiTabController.
// TODO(crbug.com/422540636): Add in view management portion once tab controller
// is ready to send information to the view controller.
class ActorOverlayViewController : public mojom::ActorOverlayPageHandler {
 public:
  explicit ActorOverlayViewController(tabs::TabInterface* tab_interface);

  ActorOverlayViewController(const ActorOverlayViewController&) = delete;
  ActorOverlayViewController& operator=(const ActorOverlayViewController&) =
      delete;

  ~ActorOverlayViewController() override;

  void BindOverlay(
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver);
  virtual ActorUiTabControllerInterface* GetTabController();

  // mojom::ActorOverlayPageHandler
  void OnHoverStatusChanged(bool is_hovering) override;

 private:
  mojo::Receiver<mojom::ActorOverlayPageHandler> receiver_{this};
  const raw_ptr<tabs::TabInterface> tab_interface_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
