// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_window_controller.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace actor::ui {

ActorUiContentsContainerController::ActorUiContentsContainerController(
    views::WebView* contents_container_view,
    ActorOverlayWebView* actor_overlay_web_view,
    ActorUiWindowController* window_controller)
    : contents_container_view_(contents_container_view),
      overlay_(actor_overlay_web_view),
      window_controller_(window_controller) {
  CHECK(contents_container_view_);
  if (features::kGlicActorUiHandoffButton.Get()) {
    handoff_button_controller_ = std::make_unique<HandoffButtonController>(
        contents_container_view_, window_controller);
  }
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

void ActorUiContentsContainerController::OnViewBoundsChanged(
    views::View* observed_view) {
  CHECK(observed_view == contents_container_view_);
  if (!contents_container_view_->web_contents()) {
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ActorUiContentsContainerController::
                                    NotifyTabControllerOnViewBoundsChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ActorUiContentsContainerController::OnWebContentsAttached(
    views::WebView* web_view) {
  if (!web_view->web_contents()) {
    return;
  }

  // Start observing on the new web contents.
  Observe(web_view->web_contents());
  view_observation_.Observe(web_view);

  // Start observing on tab scoped actor ui state changes.
  if (auto* tab =
          tabs::TabInterface::GetFromContents(web_view->web_contents())) {
    if (auto* tab_controller = ActorUiTabControllerInterface::From(tab)) {
      if (features::kGlicActorUiOverlay.Get()) {
        actor_ui_tab_controller_callback_runners_.push_back(
            tab_controller->RegisterActorOverlayStateChange(base::BindRepeating(
                &ActorUiContentsContainerController::UpdateOverlayState,
                weak_ptr_factory_.GetWeakPtr())));
        actor_ui_tab_controller_callback_runners_.push_back(
            tab_controller->RegisterActorOverlayBackgroundChange(
                base::BindRepeating(&ActorUiContentsContainerController::
                                        OnActorOverlayBackgroundChange,
                                    weak_ptr_factory_.GetWeakPtr())));
      }

      if (handoff_button_controller_) {
        actor_ui_tab_controller_callback_runners_.push_back(
            tab_controller->RegisterHandoffButtonController(
                handoff_button_controller_.get()));
        actor_ui_tab_controller_callback_runners_.push_back(
            handoff_button_controller_->RegisterTabInterface(tab));
      }

      // Record user action if associated task isn't paused or stopped
      actor::ActorKeyedService* actor_service =
          actor::ActorKeyedService::Get(web_contents()->GetBrowserContext());
      if (!actor_service) {
        return;
      }

      // Log user action if associated task isn't paused or stopped
      if (actor_service->IsActiveOnTab(*tab)) {
        actor::ui::RecordActuatingTabWebContentsAttached();
      }

      // Asynchronous post needed for the window to completely open and
      // activate before trying to show the UI components.
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ActorUiContentsContainerController::
                             NotifyTabControllerOnWebContentsAttached,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

ActorUiTabControllerInterface*
ActorUiContentsContainerController::GetActorUiTabController() {
  if (!web_contents()) {
    return nullptr;
  }
  auto* tab = tabs::TabInterface::GetFromContents(web_contents());
  if (!tab) {
    return nullptr;
  }
  return ActorUiTabControllerInterface::From(tab);
}

void ActorUiContentsContainerController::
    NotifyTabControllerOnWebContentsAttached() {
  if (auto* tab_controller = GetActorUiTabController()) {
    tab_controller->OnWebContentsAttached();
  }
}

void ActorUiContentsContainerController::
    NotifyTabControllerOnViewBoundsChanged() {
  if (auto* tab_controller = GetActorUiTabController()) {
    tab_controller->OnViewBoundsChanged();
  }
}

void ActorUiContentsContainerController::
    NotifyTabControllerOnImmersiveModeChanged() {
  if (auto* tab_controller = GetActorUiTabController()) {
    tab_controller->OnImmersiveModeChanged();
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
  view_observation_.Reset();
  actor_ui_tab_controller_callback_runners_.clear();

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
    ActorOverlayState state,
    base::OnceClosure callback) {
  if (!overlay_) {
    std::move(callback).Run();
    return;
  }

  if (is_visible) {
    overlay_->ShowUI(tabs::TabInterface::GetFromContents(
        contents_container_view_->web_contents()));
  } else {
    overlay_->CloseUI();
  }

  overlay_->SetBorderGlowVisibility(state.border_glow_visible);
  std::move(callback).Run();
}

}  // namespace actor::ui

// ActorUiWindowController Definitions:
DEFINE_USER_DATA(ActorUiWindowController);

ActorUiWindowController::ActorUiWindowController(
    BrowserWindowInterface* browser_window_interface,
    std::vector<std::pair<views::WebView*, ActorOverlayWebView*>>
        container_overlay_view_pairs)
    : browser_window_interface_(browser_window_interface),
      scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
                          *this) {
  CHECK(browser_window_interface_);
  for (const auto& pair : container_overlay_view_pairs) {
    contents_container_controllers_.push_back(
        std::make_unique<actor::ui::ActorUiContentsContainerController>(
            pair.first, pair.second, this));
  }
  InitializeImmersiveModeObserver();
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

void ActorUiWindowController::InitializeImmersiveModeObserver() {
  if (immersive_mode_observer_.IsObserving()) {
    return;
  }
  if (auto* controller =
          ImmersiveModeController::From(browser_window_interface_)) {
    immersive_mode_observer_.Observe(controller);
  } else {
    return;
  }
  if (auto* profile = browser_window_interface_->GetProfile()) {
    pref_change_registrar_.Init(profile->GetPrefs());

#if BUILDFLAG(IS_MAC)
    // Only Mac has the "Always Show Toolbar" setting.
    pref_change_registrar_.Add(
        prefs::kShowFullscreenToolbar,
        base::BindRepeating(
            &ActorUiWindowController::OnImmersiveFullscreenToolbarPrefChanged,
            weak_ptr_factory_.GetWeakPtr()));
#endif
  }
}

void ActorUiWindowController::NotifyControllersOfImmersiveChange() {
  for (const auto& controller : contents_container_controllers_) {
    controller->NotifyTabControllerOnImmersiveModeChanged();
  }
}

void ActorUiWindowController::OnImmersiveFullscreenEntered() {
  NotifyControllersOfImmersiveChange();
}

void ActorUiWindowController::OnImmersiveFullscreenExited() {
  NotifyControllersOfImmersiveChange();
}

void ActorUiWindowController::OnImmersiveRevealStarted() {
  NotifyControllersOfImmersiveChange();
}

void ActorUiWindowController::OnImmersiveRevealEnded() {
  NotifyControllersOfImmersiveChange();
}

void ActorUiWindowController::OnImmersiveFullscreenToolbarPrefChanged() {
  if (IsImmersiveModeEnabled()) {
    NotifyControllersOfImmersiveChange();
  }
}

void ActorUiWindowController::OnImmersiveModeControllerDestroyed() {
  immersive_mode_observer_.Reset();
  pref_change_registrar_.RemoveAll();
}

bool ActorUiWindowController::IsImmersiveModeEnabled() const {
  auto* controller = ImmersiveModeController::From(browser_window_interface_);
  return controller && controller->IsEnabled();
}

bool ActorUiWindowController::IsToolbarRevealed() const {
  auto* controller = ImmersiveModeController::From(browser_window_interface_);
  return controller && controller->IsRevealed();
}

bool ActorUiWindowController::IsToolbarPinned() const {
#if BUILDFLAG(IS_MAC)
  if (auto* profile = browser_window_interface_->GetProfile()) {
    return profile->GetPrefs()->GetBoolean(prefs::kShowFullscreenToolbar);
  }
#endif
  return false;
}

void ActorUiWindowController::TearDown() {
  contents_container_controllers_.clear();
}
