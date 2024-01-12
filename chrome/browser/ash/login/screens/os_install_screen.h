// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/dbus/os_install/os_install_client.h"

namespace ash {

class OsInstallScreenView;

class OsInstallScreen : public BaseScreen, public OsInstallClient::Observer {
 public:
  using TView = OsInstallScreenView;

  OsInstallScreen(base::WeakPtr<OsInstallScreenView> view,
                  const base::RepeatingClosure& exit_callback);
  OsInstallScreen(const OsInstallScreen&) = delete;
  OsInstallScreen& operator=(const OsInstallScreen&) = delete;
  ~OsInstallScreen() override;

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // OsInstallClient::Observer:
  void StatusChanged(OsInstallClient::Status status,
                     const std::string& service_log) override;

  void StartInstall();
  void RunAutoShutdownCountdown();
  void UpdateCountdownString();
  void Shutdown();

  base::WeakPtr<OsInstallScreenView> view_;

  base::TimeTicks shutdown_time_;

  // Shut down countdown timer on installation success.
  std::unique_ptr<base::RepeatingTimer> shutdown_countdown_;

  // Used for testing.
  raw_ptr<const base::TickClock> tick_clock_;

  base::ScopedObservation<OsInstallClient, OsInstallClient::Observer>
      scoped_observation_{this};

  const base::RepeatingClosure exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_
