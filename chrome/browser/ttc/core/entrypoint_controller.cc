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
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
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
  }
}

void EntrypointController::ToolbarButtonHandler() {
  ToggleSession();
}

void EntrypointController::OnTtcStateChanged(TtcState state) {
  UpdateUi(state);
}

void EntrypointController::UpdateUi(TtcState state) {
  UpdateToolbarButton(state);
}

void EntrypointController::UpdateToolbarButton(TtcState state) {
#if !BUILDFLAG(IS_ANDROID)
  PinnedToolbarActions* const pinned_actions =
      browser_->GetFeatures().pinned_toolbar_actions();
  if (!pinned_actions) {
    return;
  }

  switch (state) {
    case TtcState::kDisabled:
      // TODO(danielmendz): Remove the toolbar button entirely when TTC becomes
      // unavailable.
      break;
    case TtcState::kSessionInactive:
      pinned_actions->UpdateActionState(kActionTtcToolbar,
                                        /*is_active=*/false);
      break;
    case TtcState::kSessionActive:
      pinned_actions->UpdateActionState(kActionTtcToolbar,
                                        /*is_active=*/true);
      break;
  }
#endif
}

}  // namespace ttc
