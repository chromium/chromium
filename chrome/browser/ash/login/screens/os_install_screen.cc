// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/os_install_screen.h"

#include "ash/public/cpp/login_accelerators.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {
namespace {
constexpr const char kUserActionExitClicked[] = "os-install-exit";
constexpr const char kUserActionConfirmNextClicked[] =
    "os-install-confirm-next";
constexpr const char kUserActionErrorSendFeedbackClicked[] =
    "os-install-error-send-feedback";
constexpr const char kUserActionErrorShutdownClicked[] =
    "os-install-error-shutdown";

constexpr const base::TimeDelta kTimeTillShutdownOnSuccess = base::Seconds(60);
constexpr const base::TimeDelta kCountdownDelta = base::Milliseconds(10);
}  // namespace

OsInstallScreen::OsInstallScreen(base::WeakPtr<OsInstallScreenView> view,
                                 const base::RepeatingClosure& exit_callback)
    : BaseScreen(OsInstallScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      exit_callback_(exit_callback) {}

OsInstallScreen::~OsInstallScreen() {
  scoped_observation_.Reset();
}

void OsInstallScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show();
}

void OsInstallScreen::HideImpl() {}

void OsInstallScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionExitClicked) {
    exit_callback_.Run();
  } else if (action_id == kUserActionConfirmNextClicked) {
    StartInstall();
  } else if (action_id == kUserActionErrorSendFeedbackClicked) {
    LoginDisplayHost::default_host()->HandleAccelerator(
        LoginAcceleratorAction::kShowFeedback);
  } else if (action_id == kUserActionErrorShutdownClicked) {
    Shutdown();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void OsInstallScreen::StatusChanged(OsInstallClient::Status status,
                                    const std::string& service_log) {
  if (!view_)
    return;
  if (status == OsInstallClient::Status::Succeeded)
    RunAutoShutdownCountdown();
  view_->SetStatus(status);
  view_->SetServiceLogs(service_log);
}

void OsInstallScreen::StartInstall() {
  if (!view_)
    return;
  view_->SetStatus(OsInstallClient::Status::InProgress);

  OsInstallClient* const os_install_client = OsInstallClient::Get();

  scoped_observation_.Observe(os_install_client);
  os_install_client->StartOsInstall();
}

void OsInstallScreen::RunAutoShutdownCountdown() {
  if (shutdown_countdown_)
    return;
  shutdown_time_ = tick_clock_->NowTicks() + kTimeTillShutdownOnSuccess;
  UpdateCountdownString();
  shutdown_countdown_ = std::make_unique<base::RepeatingTimer>(tick_clock_);
  shutdown_countdown_->Start(FROM_HERE, kCountdownDelta, this,
                             &OsInstallScreen::UpdateCountdownString);
}

void OsInstallScreen::UpdateCountdownString() {
  base::TimeDelta time_left = shutdown_time_ - tick_clock_->NowTicks();
  if (!time_left.is_positive()) {
    shutdown_countdown_->Stop();
    shutdown_countdown_.reset();
    Shutdown();
  }
  if (view_)
    view_->UpdateCountdownStringWithTime(time_left);
}

void OsInstallScreen::Shutdown() {
  chromeos::PowerManagerClient::Get()->RequestShutdown(
      power_manager::REQUEST_SHUTDOWN_FOR_USER, "OS install shut down");
}

}  // namespace ash
