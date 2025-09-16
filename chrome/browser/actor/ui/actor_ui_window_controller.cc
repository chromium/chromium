// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_window_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace actor::ui {

ActorUiContentsContainerController::ActorUiContentsContainerController(
    views::WebView* contents_container_view,
    views::View* actor_overlay_view_container)
    : contents_container_view_(contents_container_view),
      actor_overlay_view_container_(actor_overlay_view_container) {
  CHECK(contents_container_view_);
  CHECK(actor_overlay_view_container_);
}

ActorUiContentsContainerController::~ActorUiContentsContainerController() =
    default;

views::WebView* ActorUiContentsContainerController::AddChildWebView(
    std::unique_ptr<views::WebView> web_view) {
  auto* web_view_result =
      actor_overlay_view_container_->AddChildView(std::move(web_view));
  MaybeUpdateContainerVisibility();
  return web_view_result;
}

[[nodiscard]] std::unique_ptr<views::WebView>
ActorUiContentsContainerController::RemoveChildWebView(
    views::WebView* web_view) {
  std::unique_ptr<views::WebView> web_view_result =
      actor_overlay_view_container_->RemoveChildViewT(web_view);
  MaybeUpdateContainerVisibility();
  return web_view_result;
}

void ActorUiContentsContainerController::MaybeUpdateContainerVisibility() {
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

bool ActorUiContentsContainerController::IsAssociatedWithWebContents(
    content::WebContents* web_contents) {
  if (!contents_container_view_) {
    return false;
  }
  if (!web_contents) {
    return false;
  }
  return contents_container_view_->web_contents() == web_contents;
}

// static
ActorUiContentsContainerController* ActorUiContentsContainerController::From(
    tabs::TabInterface* tab_interface) {
  if (!tab_interface) {
    return nullptr;
  }
  auto* window_controller =
      ActorUiWindowController::From(tab_interface->GetBrowserWindowInterface());
  if (!window_controller) {
    return nullptr;
  }

  return window_controller->GetControllerForWebContents(
      tab_interface->GetContents());
}

}  // namespace actor::ui

DEFINE_USER_DATA(ActorUiWindowController);

ActorUiWindowController::ActorUiWindowController(
    BrowserWindowInterface* browser_window_interface,
    std::vector<std::pair<views::WebView*, views::View*>>
        container_overlay_view_pairs)
    : browser_window_interface_(browser_window_interface),
      scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
                          *this) {
  for (const auto& pair : container_overlay_view_pairs) {
    contents_container_controllers_.push_back(
        std::make_unique<actor::ui::ActorUiContentsContainerController>(
            pair.first, pair.second));
  }
}

ActorUiWindowController::~ActorUiWindowController() = default;

// static
ActorUiWindowController* ActorUiWindowController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

actor::ui::ActorUiContentsContainerController*
ActorUiWindowController::GetControllerForWebContents(
    content::WebContents* web_contents) {
  for (const auto& controller : contents_container_controllers_) {
    if (controller->IsAssociatedWithWebContents(web_contents)) {
      return controller.get();
    }
  }
  return nullptr;
}
