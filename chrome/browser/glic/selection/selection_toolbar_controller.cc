// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_toolbar_controller.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/actions.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/platform_util.h"
#endif

namespace glic {

namespace {

actions::ActionItem* FindToolbarAction(BrowserWindowInterface& browser) {
  BrowserActions* const browser_actions = BrowserActions::From(&browser);
  return browser_actions ? actions::ActionManager::Get().FindAction(
                               kActionGlicSmartSelectionToolbar,
                               browser_actions->root_action_item())
                         : nullptr;
}

}  // namespace

DEFINE_USER_DATA(SelectionToolbarController);

// static
SelectionToolbarController* SelectionToolbarController::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

// static
bool SelectionToolbarController::IsToolbarActionRegistered(
    BrowserWindowInterface& browser) {
  return FindToolbarAction(browser);
}

SelectionToolbarController::SelectionToolbarController(
    BrowserWindowInterface& browser,
    GlicKeyedService& glic_service)
    : browser_(browser),
      action_item_(CHECK_DEREF(FindToolbarAction(browser))),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {
  auto update_callback = base::BindRepeating(
      &SelectionToolbarController::UpdateState, base::Unretained(this));
  subscriptions_.push_back(
      glic_service.enabling().RegisterAllowedChanged(update_callback));
  if (auto* actor_service =
          actor::ActorKeyedService::Get(browser.GetProfile())) {
    // Tasks transition state immediately when attaching or detaching tabs
    // during action execution; standalone surface membership changes are not
    // observed here.
    subscriptions_.push_back(actor_service->AddTaskStateChangedCallback(
        base::IgnoreArgs<actor::ActorTask&>(update_callback)));
  }
  subscriptions_.push_back(browser.RegisterActiveTabDidChange(
      base::BindRepeating(&SelectionToolbarController::OnActiveTabChanged,
                          base::Unretained(this))));
  OnActiveTabChanged(&browser);
}

SelectionToolbarController::~SelectionToolbarController() = default;

void SelectionToolbarController::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  eligibility_subscription_ = {};
  tabs::TabInterface* const tab = browser->GetActiveTabInterface();
  if (auto* helper =
          tab ? tabs::PageContextEligibilityHelper::From(tab) : nullptr) {
    eligibility_subscription_ = helper->RegisterEligibilityChangeCallback(
        base::IgnoreArgs<optimization_guide::PageContextEligibilityStatus>(
            base::BindRepeating(&SelectionToolbarController::UpdateState,
                                base::Unretained(this))));
  }
  UpdateState();
}

void SelectionToolbarController::UpdateState() {
  tabs::TabInterface* const tab = browser_->GetActiveTabInterface();
  auto* controller =
      tab ? SelectionOverlayController::FromTabWebContents(tab->GetContents())
          : nullptr;
  UpdateStateImpl(controller && controller->IsOverlayShowing());
}

void SelectionToolbarController::OnOverlayShowingChanged(bool is_showing) {
  UpdateStateImpl(is_showing);
}

void SelectionToolbarController::UpdateStateImpl(bool is_overlay_showing) {
  Profile* const profile = browser_->GetProfile();
  // Hidden like the Glic button, and also when signed out or in locked
  // fullscreen.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  bool visible =
      GlicEnabling::ShouldShowGlicButton(profile) && identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
#if BUILDFLAG(IS_CHROMEOS)
  visible =
      visible && !platform_util::IsBrowserLockedFullscreen(&browser_.get());
#endif
  bool enabled = false;
  if (tabs::TabInterface* const tab = browser_->GetActiveTabInterface();
      visible && tab) {
    // Stay clickable while the overlay shows so the second click closes it.
    enabled = is_overlay_showing ||
              (GlicEnabling::IsReadyForProfile(profile) &&
               SelectionOverlayController::IsTabEligibleForSession(*tab));
  }
  action_item_->SetVisible(visible);
  action_item_->SetEnabled(enabled);
  BrowserWindow* const browser_window =
      BrowserWindow::FromBrowser(&browser_.get());
  if (PinnedToolbarActions* const pinned_actions =
          browser_window ? browser_window->GetPinnedToolbarActions()
                         : nullptr) {
    pinned_actions->UpdateActionState(kActionGlicSmartSelectionToolbar,
                                      visible && is_overlay_showing);
  }
}

}  // namespace glic
