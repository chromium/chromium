// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_WINDOW_CONTROLLER_H_

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

class ActorOverlayWindowController {
 public:
  DECLARE_USER_DATA(ActorOverlayWindowController);

  explicit ActorOverlayWindowController(
      BrowserWindowInterface* browser_window_interface,
      views::View* actor_overlay_view_container);
  ~ActorOverlayWindowController();

  static ActorOverlayWindowController* From(
      BrowserWindowInterface* browser_window_interface);

  // Adds a child WebView to the overlay container, transferring ownership of
  // `web_view` to the container. The container's visibility is automatically
  // updated. Returns a raw pointer to the added WebView.
  views::WebView* AddChildWebView(std::unique_ptr<views::WebView> web_view);
  // Removes a child WebView from the overlay container and transfers its
  // ownership back to the caller. The container's visibility is automatically
  // updated.
  [[nodiscard]] std::unique_ptr<views::WebView> RemoveChildWebView(
      views::WebView* web_view);
  // Decides whether the main container should be visible based on its
  // children's state.
  void MaybeUpdateContainerVisibility();

 private:
  raw_ptr<views::View> actor_overlay_view_container_;
  ::ui::ScopedUnownedUserData<ActorOverlayWindowController> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_WINDOW_CONTROLLER_H_
