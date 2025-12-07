// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_handler.h"

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace actor::ui {

ActorOverlayHandler::ActorOverlayHandler(
    mojo::PendingRemote<mojom::ActorOverlayPage> page,
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver,
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      page_(std::move(page)),
      receiver_{this, std::move(receiver)} {}

ActorOverlayHandler::~ActorOverlayHandler() = default;

void ActorOverlayHandler::OnHoverStatusChanged(bool is_hovering) {
  if (is_hovering_ == is_hovering) {
    return;
  }
  is_hovering_ = is_hovering;
  if (auto* tab_controller = ActorUiTabControllerInterface::From(
          webui::GetTabInterface(web_contents_))) {
    tab_controller->OnOverlayHoverStatusChanged(is_hovering);
  }
}

void ActorOverlayHandler::GetCurrentBorderGlowVisibility(
    GetCurrentBorderGlowVisibilityCallback callback) {
  if (auto* tab_controller = ActorUiTabControllerInterface::From(
          webui::GetTabInterface(web_contents_))) {
    std::move(callback).Run(tab_controller->GetCurrentUiTabState()
                                .actor_overlay.border_glow_visible);
  } else {
    std::move(callback).Run(false);
  }
}

void ActorOverlayHandler::SetOverlayBackground(bool is_visible) {
  page_->SetScrimBackground(is_visible);
}

void ActorOverlayHandler::SetBorderGlowVisibility(bool is_visible) {
  page_->SetBorderGlowVisibility(is_visible);
}

}  // namespace actor::ui
