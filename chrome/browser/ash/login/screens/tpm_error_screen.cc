// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/tpm_error_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/tpm_error_screen_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {
namespace {

constexpr char kUserActionReboot[] = "reboot-system";

}  // namespace

TpmErrorScreen::TpmErrorScreen(TpmErrorView* view)
    : BaseScreen(TpmErrorView::kScreenId,
                 OobeScreenPriority::SCREEN_HARDWARE_ERROR),
      view_(view) {
  if (view_)
    view_->Bind(this);
}

TpmErrorScreen::~TpmErrorScreen() {
  if (view_)
    view_->Unbind();
}

void TpmErrorScreen::OnViewDestroyed(TpmErrorView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void TpmErrorScreen::ShowImpl() {
  if (!view_)
    return;
  DCHECK(!context()->tpm_owned_error || !context()->tpm_dbus_error);
  if (context()->tpm_owned_error) {
    view_->SetTPMOwnedErrorStep();
  } else if (context()->tpm_dbus_error) {
    view_->SetTPMDbusErrorStep();
  }
  view_->Show();
}

void TpmErrorScreen::HideImpl() {}

void TpmErrorScreen::OnUserActionDeprecated(const std::string& action_id) {
  if (action_id == kUserActionReboot) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_USER, "Signin screen");
  } else {
    BaseScreen::OnUserActionDeprecated(action_id);
  }
}

}  // namespace ash
