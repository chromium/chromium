// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_window_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

DEFINE_USER_DATA(ActorOverlayWindowController);

ActorOverlayWindowController::ActorOverlayWindowController(
    BrowserWindowInterface* browser_window_interface,
    views::View* actor_overlay_view_container)
    : actor_overlay_view_container_(actor_overlay_view_container),
      scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
                          *this) {}

ActorOverlayWindowController::~ActorOverlayWindowController() = default;

// static
ActorOverlayWindowController* ActorOverlayWindowController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

views::WebView* ActorOverlayWindowController::AddChildWebView(
    std::unique_ptr<views::WebView> web_view) {
  auto* web_view_result =
      actor_overlay_view_container_->AddChildView(std::move(web_view));
  MaybeUpdateContainerVisibility();
  return web_view_result;
}

[[nodiscard]] std::unique_ptr<views::WebView>
ActorOverlayWindowController::RemoveChildWebView(views::WebView* web_view) {
  std::unique_ptr<views::WebView> web_view_result =
      actor_overlay_view_container_->RemoveChildViewT(web_view);
  MaybeUpdateContainerVisibility();
  return web_view_result;
}

void ActorOverlayWindowController::MaybeUpdateContainerVisibility() {
  bool any_child_visible =
      std::any_of(actor_overlay_view_container_->children().begin(),
                  actor_overlay_view_container_->children().end(),
                  [](views::View* child) { return child->GetVisible(); });
  // Set the container's visibility based on whether any child is visible. Only
  // change if the state is different to avoid unnecessary updates.
  if (actor_overlay_view_container_->GetVisible() != any_child_visible) {
    actor_overlay_view_container_->SetVisible(any_child_visible);
  }
}
