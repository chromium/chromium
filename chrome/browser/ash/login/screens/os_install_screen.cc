// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/os_install_screen.h"

#include "ash/public/cpp/login_accelerators.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {
namespace {

constexpr const char kUserActionIntroNextClicked[] = "os-install-intro-next";
constexpr const char kUserActionConfirmNextClicked[] =
    "os-install-confirm-next";
constexpr const char kUserActionErrorSendFeedbackClicked[] =
    "os-install-error-send-feedback";
constexpr const char kUserActionErrorShutdownClicked[] =
    "os-install-error-shutdown";
constexpr const char kUserActionSuccessShutdownClicked[] =
    "os-install-success-shutdown";

}  // namespace

OsInstallScreen::OsInstallScreen(OsInstallScreenView* view)
    : BaseScreen(OsInstallScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view) {
  if (view_)
    view_->Bind(this);
}

OsInstallScreen::~OsInstallScreen() {
  if (view_)
    view_->Unbind();
}

void OsInstallScreen::OnViewDestroyed(OsInstallScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void OsInstallScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show();
}

void OsInstallScreen::HideImpl() {}

void OsInstallScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionIntroNextClicked) {
    view_->ShowConfirmStep();
  } else if (action_id == kUserActionConfirmNextClicked) {
    view_->StartInstall();
  } else if (action_id == kUserActionErrorSendFeedbackClicked) {
    LoginDisplayHost::default_host()->HandleAccelerator(
        LoginAcceleratorAction::kShowFeedback);
  } else if (action_id == kUserActionErrorShutdownClicked ||
             action_id == kUserActionSuccessShutdownClicked) {
    Shutdown();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void OsInstallScreen::Shutdown() {
  chromeos::PowerManagerClient::Get()->RequestShutdown(
      power_manager::REQUEST_SHUTDOWN_FOR_USER, "OS install shut down");
}

}  // namespace ash
