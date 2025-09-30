// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class ActorOverlayWebView;

namespace views {
class WebView;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;
namespace actor::ui {

// Manages the actor ui components for a single contents container (e.g., a
// single tab's content area). In split-view mode, there will be multiple
// instances of this class, one for each content area.
class ActorUiContentsContainerController : public content::WebContentsObserver {
 public:
  explicit ActorUiContentsContainerController(
      views::WebView* contents_container_view,
      ActorOverlayWebView* actor_overlay_web_view);
  ActorUiContentsContainerController(
      const ActorUiContentsContainerController&) = delete;
  ActorUiContentsContainerController& operator=(
      const ActorUiContentsContainerController&) = delete;
  ~ActorUiContentsContainerController() override;

  views::WebView* contents_container_view() { return contents_container_view_; }

  // Called whenever web contents are attached to a `web_view`.
  void OnWebContentsAttached(views::WebView* web_view);

  // Called whenever web contents are detached from a `web_view`.
  void OnWebContentsDetached(views::WebView* web_view);

  // Updates the overlay_ state.
  void UpdateOverlayState(bool is_visible, ActorOverlayState state);

 private:
  // Notifies the respective tab controller that a new web contents has been
  // attached.
  void NotifyTabControllerOnWebContentsAttached();
  // Notified whenever the overlay background status changes.
  void OnActorOverlayBackgroundChange(bool is_visible);

  std::vector<base::CallbackListSubscription>
      web_contents_callback_subscriptions_;
  std::vector<base::CallbackListSubscription>
      actor_ui_tab_controller_callback_subscriptions_;
  raw_ptr<views::WebView> contents_container_view_ = nullptr;
  raw_ptr<ActorOverlayWebView> overlay_ = nullptr;

  base::WeakPtrFactory<ActorUiContentsContainerController> weak_ptr_factory_{
      this};
};

}  // namespace actor::ui

class ActorUiWindowController {
 public:
  DECLARE_USER_DATA(ActorUiWindowController);

  explicit ActorUiWindowController(
      BrowserWindowInterface* browser_window_interface,
      std::vector<std::pair<views::WebView*, ActorOverlayWebView*>>
          container_overlay_view_pairs);
  ~ActorUiWindowController();

  static ActorUiWindowController* From(
      BrowserWindowInterface* browser_window_interface);

  void TearDown();

  actor::ui::ActorUiContentsContainerController* GetControllerForWebContents(
      content::WebContents* web_contents);

 private:
  // Vector of all owned ContentsContainerControllers: One for each content
  // area..
  std::vector<std::unique_ptr<actor::ui::ActorUiContentsContainerController>>
      contents_container_controllers_;
  ::ui::ScopedUnownedUserData<ActorUiWindowController> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_
