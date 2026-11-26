// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/entrypoint_controller.h"

#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/prefs.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/ttc/ttc_fre_dialog.h"
#include "components/prefs/pref_service.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"

namespace ttc {

namespace {

// TODO(b/588446594): Link to TTC's own help article once there is one.
constexpr char kLearnMoreUrl[] = "https://www.google.com";

}  // namespace

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
  browser_did_close_subscription_ =
      browser_->RegisterBrowserDidClose(base::BindRepeating(
          &EntrypointController::OnBrowserDidClose, base::Unretained(this)));

  UpdateUi(service_->GetState());
}

EntrypointController::~EntrypointController() = default;

void EntrypointController::ToggleSession() {
  if (service_->is_session_active()) {
    service_->EndSession();
  } else {
    StartSession();
  }
}

void EntrypointController::StartSession() {
  if (service_->GetState() != ServiceState::kSessionInactive) {
    return;
  }
  if (fre_dialog_) {
    fre_dialog_->Activate();
    return;
  }
  if (NeedsFre()) {
    fre_dialog_ = std::make_unique<TtcFreDialog>(
        CHECK_DEREF(browser_->GetWindow()).GetNativeWindow(),
        base::BindOnce(&EntrypointController::OnFreDialogClosed,
                       base::Unretained(this)),
        base::BindRepeating(&EntrypointController::OnFreLearnMoreClicked,
                            base::Unretained(this)));
    fre_dialog_->Show();
    return;
  }
  service_->StartSession();
}

bool EntrypointController::NeedsFre() const {
  return CHECK_DEREF(browser_->GetProfile()->GetPrefs())
             .GetInteger(prefs::kTtcFreAcceptedVersion) <
         prefs::kCurrentFreVersion;
}

void EntrypointController::OnFreDialogClosed(bool accepted) {
  fre_dialog_.reset();
  if (!accepted || service_->GetState() != ServiceState::kSessionInactive) {
    return;
  }
  CHECK_DEREF(browser_->GetProfile()->GetPrefs())
      .SetInteger(prefs::kTtcFreAcceptedVersion, prefs::kCurrentFreVersion);
  service_->StartSession();
}

void EntrypointController::OnFreLearnMoreClicked() {
  browser_->OpenGURL(GURL(kLearnMoreUrl),
                     ui::mojom::WindowOpenDisposition::kNewWindow);
}

void EntrypointController::OnBrowserDidClose(BrowserWindowInterface* browser) {
  fre_dialog_.reset();
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
  StartSession();
}

void EntrypointController::OnTtcStateChanged(ServiceState state) {
  UpdateUi(state);
}

void EntrypointController::UpdateUi(ServiceState state) {
  // A session started from another window, or TTC became unavailable.
  if (state != ServiceState::kSessionInactive) {
    fre_dialog_.reset();
  }
  UpdateToolbarButton(state);
}

void EntrypointController::UpdateToolbarButton(ServiceState state) {
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
}

}  // namespace ttc
