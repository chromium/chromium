// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_window_controller.h"

#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace actor::ui {

ActorUiContentsContainerController::ActorUiContentsContainerController(
    views::WebView* contents_container_view,
    ActorOverlayWebView* actor_overlay_web_view)
    : contents_container_view_(contents_container_view),
      overlay_(actor_overlay_web_view) {
  CHECK(contents_container_view_);
  web_contents_callback_subscriptions_.push_back(
      contents_container_view_->AddWebContentsAttachedCallback(
          base::BindRepeating(
              &ActorUiContentsContainerController::OnWebContentsAttached,
              weak_ptr_factory_.GetWeakPtr())));
  web_contents_callback_subscriptions_.push_back(
      contents_container_view_->AddWebContentsDetachedCallback(
          base::BindRepeating(
              &ActorUiContentsContainerController::OnWebContentsDetached,
              weak_ptr_factory_.GetWeakPtr())));
  OnWebContentsAttached(contents_container_view_);
}

ActorUiContentsContainerController::~ActorUiContentsContainerController() =
    default;

void ActorUiContentsContainerController::OnWebContentsAttached(
    views::WebView* web_view) {
  if (!web_view->web_contents()) {
    return;
  }

  // Start observing on the new web contents.
  Observe(web_view->web_contents());

  // Start observing on tab scoped actor ui state changes.
  if (auto* tab =
          tabs::TabInterface::GetFromContents(web_view->web_contents())) {
    if (auto* tab_controller = ActorUiTabControllerInterface::From(tab)) {
      if (features::kGlicActorUiOverlay.Get()) {
        actor_ui_tab_controller_callback_subscriptions_.push_back(
            tab_controller->RegisterActorOverlayStateChange(base::BindRepeating(
                &ActorUiContentsContainerController::UpdateOverlayState,
                weak_ptr_factory_.GetWeakPtr())));
      }
      actor_ui_tab_controller_callback_subscriptions_.push_back(
          tab_controller->RegisterActorOverlayBackgroundChange(
              base::BindRepeating(&ActorUiContentsContainerController::
                                      OnActorOverlayBackgroundChange,
                                  weak_ptr_factory_.GetWeakPtr())));
      tab_controller->OnWebContentsAttached();
    }
  }
}

void ActorUiContentsContainerController::OnWebContentsDetached(
    views::WebView* web_view) {
  if (!web_view->web_contents()) {
    return;
  }

  // Stop observing on web contents and clear all subscriptions related to a
  // tab.
  Observe(nullptr);
  actor_ui_tab_controller_callback_subscriptions_.clear();

  if (overlay_) {
    overlay_->CloseUI();
  }
}

void ActorUiContentsContainerController::OnActorOverlayBackgroundChange(
    bool is_visible) {
  if (!overlay_) {
    return;
  }
  overlay_->SetOverlayBackground(is_visible);
}

void ActorUiContentsContainerController::UpdateOverlayState(
    bool is_visible,
    ActorOverlayState state) {
  if (!overlay_) {
    return;
  }

  if (is_visible) {
    overlay_->ShowUI(tabs::TabInterface::GetFromContents(
        contents_container_view_->web_contents()));
  } else {
    overlay_->CloseUI();
  }
}

}  // namespace actor::ui

// ActorUiWindowController Definitions:
DEFINE_USER_DATA(ActorUiWindowController);

ActorUiWindowController::ActorUiWindowController(
    BrowserWindowInterface* browser_window_interface,
    std::vector<std::pair<views::WebView*, ActorOverlayWebView*>>
        container_overlay_view_pairs)
    : scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
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
    if (web_contents == controller->web_contents()) {
      return controller.get();
    }
  }
  return nullptr;
}

void ActorUiWindowController::TearDown() {
  contents_container_controllers_.clear();
}
