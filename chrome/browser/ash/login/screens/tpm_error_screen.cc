// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/tpm_error_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

namespace {

constexpr char kUserActionReboot[] = "reboot-system";

}  // namespace

TpmErrorScreen::TpmErrorScreen(base::WeakPtr<TpmErrorView> view)
    : BaseScreen(TpmErrorView::kScreenId,
                 OobeScreenPriority::SCREEN_HARDWARE_ERROR),
      view_(std::move(view)) {}

TpmErrorScreen::~TpmErrorScreen() = default;

void TpmErrorScreen::ShowImpl() {
  if (!view_)
    return;
  // Set the OobeScreenPending variable to its default value. This will allow
  // users to resume the out-of-box experience (OOBE) after a restart without
  // being blocked. we set this when showing screen as user may use the hardware
  // button to reboot insread of  clicking the UI button
  StartupUtils::SaveOobePendingScreen("");
  DCHECK(!context()->tpm_owned_error || !context()->tpm_dbus_error);
  if (context()->tpm_owned_error) {
    view_->SetTPMOwnedErrorStep();
  } else if (context()->tpm_dbus_error) {
    view_->SetTPMDbusErrorStep();
  }
  view_->Show();
}

void TpmErrorScreen::HideImpl() {}

void TpmErrorScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionReboot) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_USER, "Signin screen");
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
