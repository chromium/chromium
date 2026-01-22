// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class ActorOverlayWebView;

namespace views {
class WebView;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;
class ActorUiWindowController;
namespace actor::ui {

class ActorUiTabControllerInterface;
class HandoffButtonController;

// Manages the actor ui components for a single contents container (e.g., a
// single tab's content area). In split-view mode, there will be multiple
// instances of this class, one for each content area.
class ActorUiContentsContainerController : public content::WebContentsObserver,
                                           public views::ViewObserver {
 public:
  explicit ActorUiContentsContainerController(
      views::WebView* contents_container_view,
      ActorOverlayWebView* actor_overlay_web_view,
      ActorUiWindowController* window_controller);
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
  void UpdateOverlayState(bool is_visible,
                          ActorOverlayState state,
                          base::OnceClosure callback);

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // Called by the WindowController when Immersive state changes.
  void NotifyTabControllerOnImmersiveModeChanged();

 private:
  // Gets the ActorUiTabController associated with the contentsContainer's
  // webcontents.
  ActorUiTabControllerInterface* GetActorUiTabController();
  // Notifies the respective tab controller that a new web contents has been
  // attached.
  void NotifyTabControllerOnWebContentsAttached();
  // Notifies the respective tab controller that viewBounds changed.
  void NotifyTabControllerOnViewBoundsChanged();
  // Notified whenever the overlay background status changes.
  void OnActorOverlayBackgroundChange(bool is_visible);

  std::vector<base::CallbackListSubscription>
      web_contents_callback_subscriptions_;
  std::vector<base::ScopedClosureRunner>
      actor_ui_tab_controller_callback_runners_;
  raw_ptr<views::WebView> contents_container_view_ = nullptr;
  raw_ptr<ActorOverlayWebView> overlay_ = nullptr;
  raw_ptr<ActorUiWindowController> window_controller_ = nullptr;

  std::unique_ptr<HandoffButtonController> handoff_button_controller_;

  // Observer to get notifications when the view changes.
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};

  base::WeakPtrFactory<ActorUiContentsContainerController> weak_ptr_factory_{
      this};
};

}  // namespace actor::ui

class ActorUiWindowController : public ImmersiveModeController::Observer {
 public:
  DECLARE_USER_DATA(ActorUiWindowController);

  explicit ActorUiWindowController(
      BrowserWindowInterface* browser_window_interface,
      std::vector<std::pair<views::WebView*, ActorOverlayWebView*>>
          container_overlay_view_pairs);
  ~ActorUiWindowController() override;

  static ActorUiWindowController* From(
      BrowserWindowInterface* browser_window_interface);

  void TearDown();

  actor::ui::ActorUiContentsContainerController* GetControllerForWebContents(
      content::WebContents* web_contents);

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;
  void OnImmersiveModeControllerDestroyed() override;

  bool IsImmersiveModeEnabled() const;
  bool IsToolbarRevealed() const;
  bool IsToolbarPinned() const;

 private:
  void InitializeImmersiveModeObserver();
  void NotifyControllersOfImmersiveChange();
  void OnImmersiveFullscreenToolbarPrefChanged();

  // Helper to run the immersive change notification asynchronously.
  void NotifyControllersOfImmersiveChangeInternal();

  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observer_{this};

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Vector of all owned ContentsContainerControllers: One for each content
  // area..
  std::vector<std::unique_ptr<actor::ui::ActorUiContentsContainerController>>
      contents_container_controllers_;
  ::ui::ScopedUnownedUserData<ActorUiWindowController> scoped_data_holder_;

  base::WeakPtrFactory<ActorUiWindowController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_WINDOW_CONTROLLER_H_
