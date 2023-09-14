// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/tpm_error_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

namespace {

constexpr char kUserActionReboot[] = "reboot-system";
constexpr char kUserActionSkip[] = "tpm-skip";

}  // namespace

// static
std::string TpmErrorScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kSkip:
      return "Skip";
  }
}

TpmErrorScreen::TpmErrorScreen(base::WeakPtr<TpmErrorView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(TpmErrorView::kScreenId,
                 OobeScreenPriority::SCREEN_HARDWARE_ERROR),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

TpmErrorScreen::~TpmErrorScreen() = default;

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

void TpmErrorScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionReboot) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_USER, "Signin screen");
  } else if (action_id == kUserActionSkip) {
    exit_callback_.Run(Result::kSkip);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
