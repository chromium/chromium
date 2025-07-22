// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"

namespace actor::ui {

ActorOverlayViewController::ActorOverlayViewController(
    tabs::TabInterface* tab_interface)
    : tab_interface_(tab_interface) {}

ActorOverlayViewController::~ActorOverlayViewController() = default;

void ActorOverlayViewController::BindOverlay(
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) {
  receiver_.Bind(std::move(receiver));
}

ActorUiTabControllerInterface* ActorOverlayViewController::GetTabController() {
  return tab_interface_->GetTabFeatures()->actor_ui_tab_controller();
}

// TODO(crbug.com/422540636): Not sufficient to determine when the handoff
// button should be visible. Look into ways of tracking mouse movements
// directly.
void ActorOverlayViewController::OnHoverStatusChanged(bool is_hovering) {
  GetTabController()->SetHandoffButtonVisibility(is_hovering);
}

}  // namespace actor::ui
