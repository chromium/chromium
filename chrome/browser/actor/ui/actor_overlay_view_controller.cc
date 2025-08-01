// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"

namespace actor::ui {

ActorOverlayViewController::ActorOverlayViewController(
    tabs::TabInterface& tab_interface)
    : tab_interface_(tab_interface) {}

ActorOverlayViewController::~ActorOverlayViewController() = default;

void ActorOverlayViewController::BindOverlay(
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) {
  receiver_.Bind(std::move(receiver));
}

ActorUiTabControllerInterface* ActorOverlayViewController::GetTabController() {
  return tab_interface_->GetTabFeatures()->actor_ui_tab_controller();
}

// TODO(crbug.com/422540636): Might not be sufficient to determine when the
// handoff button should be visible. Look into ways of tracking mouse movements
// directly.
void ActorOverlayViewController::OnHoverStatusChanged(bool is_hovering) {
  GetTabController()->SetOverlayHoverStatus(is_hovering);
}

void ActorOverlayViewController::UpdateState(const ActorOverlayState& state,
                                             bool is_visible) {
  if (is_visible) {
    // Create the WebView only if it doesn't already exist (either attached or
    // managed).
    if (!overlay_web_view_ && !managed_overlay_web_view_) {
      CreateWebView();
    }
    ShowWebView();
  } else {
    HideWebView();
  }
}

void ActorOverlayViewController::AttachManagedWebViewToWindowController() {
  if (!managed_overlay_web_view_) {
    return;  // No WebView to attach.
  }
  CHECK(actor_overlay_window_controller_);
  // Transfer ownership from `managed_overlay_web_view_` to the window
  // controller's container.
  overlay_web_view_ = actor_overlay_window_controller_->AddChildWebView(
      std::move(managed_overlay_web_view_));
  // Ensure the newly attached WebView is initially hidden.
  overlay_web_view_->SetVisible(false);
  // Clear the unique_ptr as ownership has been transferred.
  managed_overlay_web_view_ = nullptr;
}

void ActorOverlayViewController::SetWindowController(
    ActorOverlayWindowController* controller) {
  CHECK(controller);
  actor_overlay_window_controller_ = controller;
  // If a WebView was previously detached, re-attach it to the new window
  // controller.
  AttachManagedWebViewToWindowController();
}

void ActorOverlayViewController::NullifyWebView() {
  if (!overlay_web_view_) {
    return;
  }
  // Reclaim ownership of the WebView from the window controller's container.
  managed_overlay_web_view_ =
      actor_overlay_window_controller_->RemoveChildWebView(overlay_web_view_);
  // Clear the raw pointer since the WebView is no longer attached.
  overlay_web_view_ = nullptr;
}

void ActorOverlayViewController::CreateWebView() {
  // These CHECKs enforce that this function is only for initial creation, not
  // for re-attaching an already existing WebView.
  CHECK(!overlay_web_view_);
  CHECK(!managed_overlay_web_view_);

  content::BrowserContext* browser_context =
      tab_interface_->GetContents()->GetBrowserContext();
  managed_overlay_web_view_ = std::make_unique<views::WebView>(browser_context);
  content::WebContents* web_view_contents =
      managed_overlay_web_view_->GetWebContents();

  // Make the WebUI background transparent so it can act as an overlay.
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_view_contents, SK_ColorTRANSPARENT);
  // Associates the WebView's WebContents with its corresponding TabInterface.
  // This allows the WebUI (ActorOverlayUI) to access the correct tab-scoped
  // controllers (e.g., ActorUiTabController) for Mojo communication.
  webui::SetTabInterface(web_view_contents, &*tab_interface_);

  managed_overlay_web_view_->LoadInitialURL(
      GURL(chrome::kChromeUIActorOverlayURL));
  managed_overlay_web_view_->SetVisible(false);
  // Attach the newly created WebView to the window controller.
  AttachManagedWebViewToWindowController();
}

void ActorOverlayViewController::ShowWebView() {
  // Disable mouse and keyboard inputs to the underlying contents.
  scoped_ignore_input_events_ =
      tab_interface_->GetContents()->IgnoreInputEvents(std::nullopt);
  // Ensure the overlay WebView exists before showing it.
  CHECK(overlay_web_view_);
  overlay_web_view_->SetVisible(true);
  actor_overlay_window_controller_->MaybeUpdateContainerVisibility();
}

// TODO(crbug.com/422540636): Look into if HideWebView is called when the Actor
// Task fails.
void ActorOverlayViewController::HideWebView() {
  // Only hide if the WebView is currently attached.
  if (!overlay_web_view_) {
    return;
  }
  overlay_web_view_->SetVisible(false);
  actor_overlay_window_controller_->MaybeUpdateContainerVisibility();
  // Re-enable mouse and keyboard events to the underlying web contents by
  // resetting the ScopedIgnoreInputEvents object.
  scoped_ignore_input_events_.reset();
}

}  // namespace actor::ui
