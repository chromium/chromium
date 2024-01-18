// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/install_attributes_error_screen.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/install_attributes_error_screen_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

namespace {

constexpr char kUserActionResetPowerwashPressed[] = "powerwash-pressed";
constexpr char kUserActionReboot[] = "reboot-system";

}  // namespace

InstallAttributesErrorScreen::InstallAttributesErrorScreen(
    base::WeakPtr<InstallAttributesErrorView> view)
    : BaseScreen(InstallAttributesErrorView::kScreenId,
                 OobeScreenPriority::SCREEN_HARDWARE_ERROR),
      view_(std::move(view)) {}

InstallAttributesErrorScreen::~InstallAttributesErrorScreen() = default;

void InstallAttributesErrorScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();
}

void InstallAttributesErrorScreen::HideImpl() {}

void InstallAttributesErrorScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionResetPowerwashPressed) {
    SessionManagerClient::Get()->StartDeviceWipe(base::DoNothing());
  } else if (action_id == kUserActionReboot) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_USER, "Install attributes error");
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
