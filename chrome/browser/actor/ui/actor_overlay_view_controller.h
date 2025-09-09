// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_

#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace views {
class WebView;
}

namespace actor::ui {

class ActorOverlayContentsContainerController;

// Manages the browser-side UI and lifecycle of the Actor Overlay for a specific
// tab. This controller implements mojom::ActorOverlayPageHandler to receive
// events from the WebUI. It orchestrates the creation, display, and hiding of
// the overlay's views::WebView, managing its attachment to the
// ActorOverlayWindowController (which is window-scoped) and controlling input
// to the underlying web content.
class ActorOverlayViewController : public mojom::ActorOverlayPageHandler {
 public:
  explicit ActorOverlayViewController(tabs::TabInterface& tab_interface);

  ActorOverlayViewController(const ActorOverlayViewController&) = delete;
  ActorOverlayViewController& operator=(const ActorOverlayViewController&) =
      delete;

  ~ActorOverlayViewController() override;

  // Binds the Mojo receiver to enable communication from the WebUI. Called by
  // ActorUiTabController.
  virtual void BindOverlay(
      mojo::PendingRemote<mojom::ActorOverlayPage> page,
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver);

  // Updates the visibility and state of the Actor Overlay for this tab. Called
  // by ActorUiTabController when the tab's active status or foreground status
  // changes. It orchestrates the creation, showing, or hiding of the overlay
  // WebView.
  virtual void UpdateState(const ActorOverlayState& state, bool is_visible);

  // mojom::ActorOverlayPageHandler
  // Notifies the ActorUiTabController that the user's hovering status over the
  // overlay has changed. Called by the ActorOverlay WebUI (renderer-side).
  void OnHoverStatusChanged(bool is_hovering) override;

  // mojom::ActorOverlayPage
  // Forwards the scrim background visibility to WebUI.
  virtual void SetScrimBackground(bool is_visible);

 private:
  // Tab subscriptions:
  // Called when the tab is detached.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  // Detaches the overlay's WebView from its current window controller and
  // reclaims its ownership. Called when the tab is about to detach from a
  // window. This is important when tab's that are being actuated, move between
  // different windows.
  void NullifyWebView();
  // Creates a new WebView instance for the overlay. Called by UpdateState when
  // the overlay needs to be shown for the first time for this tab. It also
  // attaches the WebView to the window controller.
  void CreateWebView();

  // Makes the overlay WebView visible and disables input to the underlying web
  // contents. Called by UpdateState.
  void ShowWebView();

  // Hides the overlay WebView and re-enables input to the underlying web
  // contents. Called by UpdateState.
  void HideWebView();

  // Attaches a WebView (either newly created or previously detached) to the
  // current ActorOverlayWindowController. Called always before a view is shown
  // to ensure the attachment is done. Will do nothing if the view is already
  // attached.
  virtual void AttachManagedWebViewToWindowController();

  // Manages the lifetime of the WebContents input event ignoring state.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;

  // A raw pointer to the views::WebView that is currently attached to the
  // ActorOverlayWindowController's parent container View. This represents the
  // "active" or "displayed" (though possibly hidden) overlay WebView for this
  // tab in the current window.
  raw_ptr<views::WebView> overlay_web_view_ = nullptr;

  // The controller for the contents container that this overlay is associated
  // with. This is used to show and hide the overlay WebView.
  raw_ptr<ActorOverlayContentsContainerController>
      contents_container_controller_ = nullptr;

  // A unique_ptr that holds ownership of the views::WebView when it is detached
  // from the browser's views hierarchy (e.g., when a tab is dragged out of a
  // window). This WebView is managed by the view controller and is awaiting
  // re-attachment to a new window's hierarchy. This happens when NullifyWebView
  // and SetWindowController are called by the Tab Controller after tab detach
  // and insert events are received.
  std::unique_ptr<views::WebView> managed_overlay_web_view_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  mojo::Receiver<mojom::ActorOverlayPageHandler> receiver_{this};
  mojo::Remote<mojom::ActorOverlayPage> page_;
  const raw_ref<tabs::TabInterface> tab_interface_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
