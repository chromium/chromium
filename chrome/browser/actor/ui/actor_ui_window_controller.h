// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace views {
class View;
class WebView;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}

class BrowserWindowInterface;

namespace actor::ui {

// Manages the actor overlay for a single contents container (e.g., a single
// tab's content area). In split-view mode, there will be multiple instances of
// this class, one for each content area.
class ActorUiContentsContainerController {
 public:
  explicit ActorUiContentsContainerController(
      views::WebView* contents_container_view,
      views::View* actor_overlay_view_container);
  ~ActorUiContentsContainerController();

  static ActorUiContentsContainerController* From(
      tabs::TabInterface* tab_interface);

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

  // Returns true if this controller is associated with the given
  // `web_contents`.
  bool IsAssociatedWithWebContents(content::WebContents* web_contents);

 private:
  raw_ptr<views::WebView> contents_container_view_ = nullptr;
  raw_ptr<views::View> actor_overlay_view_container_ = nullptr;
};

}  // namespace actor::ui

// Manages the actor overlay for a browser window. This controller is
// responsible for creating and managing the
// `ActorUiContentsContainerController`s for each content area in the
// window.
class ActorUiWindowController {
 public:
  DECLARE_USER_DATA(ActorUiWindowController);

  explicit ActorUiWindowController(
      BrowserWindowInterface* browser_window_interface,
      std::vector<std::pair<views::WebView*, views::View*>>
          container_overlay_view_pairs);
  ~ActorUiWindowController();

  static ActorUiWindowController* From(
      BrowserWindowInterface* browser_window_interface);

  actor::ui::ActorUiContentsContainerController* GetControllerForWebContents(
      content::WebContents* web_contents);

 private:
  // Vector of all owned ContentsContainerControllers: One for each content
  // area..
  std::vector<std::unique_ptr<actor::ui::ActorUiContentsContainerController>>
      contents_container_controllers_;
  // The `BrowserWindowInterface` that owns this controller.
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
  ::ui::ScopedUnownedUserData<ActorUiWindowController> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_
