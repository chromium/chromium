// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/entrypoint_controller.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#endif

namespace ttc {

DEFINE_USER_DATA(EntrypointController);

// static
EntrypointController* EntrypointController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

EntrypointController::EntrypointController(BrowserWindowInterface& browser,
                                           TtcKeyedService& service)
    : browser_(browser),
      service_(service),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {
  ttc_state_subscription_ =
      service_->RegisterStateChangedCallback(base::BindRepeating(
          &EntrypointController::OnTtcStateChanged, base::Unretained(this)));

  UpdateUi(service_->GetState());
}

EntrypointController::~EntrypointController() = default;

void EntrypointController::ToggleSession() {
  if (service_->is_session_active()) {
    service_->EndSession();
  } else {
    service_->StartSession();
  }
}

void EntrypointController::EntrypointHandler(EntrypointType type) {
  switch (type) {
    case EntrypointType::kToolbarButton:
      ToolbarButtonHandler();
      break;
    case EntrypointType::kAppMenu:
      AppMenuHandler();
      break;
  }
}

void EntrypointController::ToolbarButtonHandler() {
  ToggleSession();
}

void EntrypointController::AppMenuHandler() {
  CHECK(!service_->is_session_active());
  service_->StartSession();
}

void EntrypointController::OnTtcStateChanged(ServiceState state) {
  UpdateUi(state);
}

void EntrypointController::UpdateUi(ServiceState state) {
  UpdateToolbarButton(state);
}

void EntrypointController::UpdateToolbarButton(ServiceState state) {
#if !BUILDFLAG(IS_ANDROID)
  // Driven by OnTtcStateChanged(), which can fire while the window is going
  // away.
  BrowserWindow* const browser_window =
      BrowserWindow::FromBrowser(&browser_.get());
  PinnedToolbarActions* const pinned_actions =
      browser_window ? browser_window->GetPinnedToolbarActions() : nullptr;
  if (!pinned_actions) {
    return;
  }

  switch (state) {
    case ServiceState::kProfileIneligible:
      // TODO(danielmendz): Remove the toolbar button entirely when TTC becomes
      // unavailable.
      break;
    case ServiceState::kSessionInactive:
      pinned_actions->UpdateActionState(kActionTtcToolbar,
                                        /*is_active=*/false);
      break;
    case ServiceState::kSessionActive:
      pinned_actions->UpdateActionState(kActionTtcToolbar,
                                        /*is_active=*/true);
      break;
  }
#endif
}

}  // namespace ttc
