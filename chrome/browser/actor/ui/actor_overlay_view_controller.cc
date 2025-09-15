// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"

#include "chrome/browser/actor/ui/actor_overlay_window_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"

namespace actor::ui {

using tabs::TabInterface;

ActorOverlayViewController::ActorOverlayViewController(
    TabInterface& tab_interface)
    : tab_interface_(tab_interface) {
  if (features::kGlicActorUiOverlay.Get()) {
    tab_subscriptions_.push_back(tab_interface_->RegisterWillDetach(
        base::BindRepeating(&ActorOverlayViewController::OnTabWillDetach,
                            base::Unretained(this))));
  }
}

ActorOverlayViewController::~ActorOverlayViewController() = default;

void ActorOverlayViewController::BindOverlay(
    mojo::PendingRemote<mojom::ActorOverlayPage> page,
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) {
  receiver_.Bind(std::move(receiver));
  page_.Bind(std::move(page));
}

bool ActorOverlayViewController::IsHovering() {
  return is_hovering_;
}

// TODO(crbug.com/422540636): Might not be sufficient to determine when the
// handoff button should be visible. Look into ways of tracking mouse movements
// directly.
void ActorOverlayViewController::OnHoverStatusChanged(bool is_hovering) {
  if (is_hovering_ == is_hovering) {
    return;
  }
  is_hovering_ = is_hovering;
  ActorUiTabControllerInterface::From(&tab_interface_.get())
      ->OnOverlayHoverStatusChanged();
}

void ActorOverlayViewController::SetScrimBackground(bool is_visible) {
  page_->SetScrimBackground(is_visible);
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

// TODO(crbug.com/436662421): Clean up the null checks for
// actor_overlay_window_controller_. These were added to prevent crashes in test
// scenarios where a controller is null. Once those tests are cleaned up, all
// the null checks on if the window controller / web_views in this file can be
// removed and we can assume they should be available with a CHECK again.
void ActorOverlayViewController::AttachManagedWebViewToWindowController() {
  // TODO(crbug.com/433999185): WebView child management upon tabs moving
  // (into split view mode and out of split view mode) is not covered in this
  // CL, and will be in a future one.
  // No webview to attach
  if (!managed_overlay_web_view_) {
    return;
  }

  auto* contents_controller = ActorOverlayContentsContainerController::From(
      base::to_address(tab_interface_));
  // No contents container controller to use.
  if (!contents_controller) {
    return;
  }

  // Keep track of the contents container controller so this can be used to show
  // and hide the webview.
  contents_container_controller_ = contents_controller;
  // Transfer ownership from `managed_overlay_web_view_` to the window
  // controller's container.

  overlay_web_view_ = contents_controller->AddChildWebView(
      std::move(managed_overlay_web_view_));
  // Ensure the newly attached WebView is initially hidden.
  overlay_web_view_->SetVisible(false);
  // Clear the unique_ptr as ownership has been transferred.
  managed_overlay_web_view_ = nullptr;
}

void ActorOverlayViewController::NullifyWebView() {
  // No controller or webview to remove.
  if (!overlay_web_view_ || !contents_container_controller_) {
    return;
  }

  // Reclaim ownership of the WebView from the contents container controller's
  // container.
  managed_overlay_web_view_ =
      contents_container_controller_->RemoveChildWebView(overlay_web_view_);
  // Clear the raw pointers since the WebView is no longer attached.
  overlay_web_view_ = nullptr;
  // The controller is no longer valid as the webview is no longer attached.
  contents_container_controller_ = nullptr;
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
}

void ActorOverlayViewController::ShowWebView() {
  // Attach if one was created prior to a tab moving.
  AttachManagedWebViewToWindowController();
  // Only show if the WebView is currently attached.
  if (!overlay_web_view_) {
    return;
  }
  CHECK(contents_container_controller_);
  // Disable mouse and keyboard inputs to the underlying contents.
  scoped_ignore_input_events_ =
      tab_interface_->GetContents()->IgnoreInputEvents(std::nullopt);
  // Prevent assistive technologies from interacting with the underlying page.
  if (auto* content_web_view =
          tab_interface_->GetBrowserWindowInterface()->GetWebView()) {
    content_web_view->GetViewAccessibility().SetIsIgnored(true);
  }
  overlay_web_view_->SetVisible(true);
  contents_container_controller_->MaybeUpdateContainerVisibility();
}

// TODO(crbug.com/422540636): Look into if HideWebView is called when the Actor
// Task fails.
void ActorOverlayViewController::HideWebView() {
  // Only hide if the WebView is currently attached.
  if (!overlay_web_view_) {
    return;
  }
  CHECK(contents_container_controller_);
  overlay_web_view_->SetVisible(false);
  contents_container_controller_->MaybeUpdateContainerVisibility();
  // Re-enable mouse and keyboard events to the underlying web contents by
  // resetting the ScopedIgnoreInputEvents object.
  scoped_ignore_input_events_.reset();
  // Allow assistive technologies to interact with the underlying page.
  if (auto* content_web_view =
          tab_interface_->GetBrowserWindowInterface()->GetWebView()) {
    content_web_view->GetViewAccessibility().SetIsIgnored(false);
  }
}

void ActorOverlayViewController::OnTabWillDetach(
    TabInterface* tab,
    TabInterface::DetachReason reason) {
  NullifyWebView();
}

}  // namespace actor::ui
