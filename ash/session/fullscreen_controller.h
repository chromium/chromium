// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_FULLSCREEN_CONTROLLER_H_
#define ASH_SESSION_FULLSCREEN_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"

class PrefRegistrySimple;

namespace chromeos {
class KeepFullscreenForUrlChecker;
}  // namespace chromeos

namespace ash {

class SessionControllerImpl;
class FullscreenNotificationBubble;

class FullscreenController : public chromeos::PowerManagerClient::Observer {
 public:
  explicit FullscreenController(SessionControllerImpl* session_controller);
  FullscreenController(const FullscreenController&) = delete;
  FullscreenController& operator=(const FullscreenController&) = delete;

  ~FullscreenController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Checks the window state, user pref and, if required, sends a request to
  // Lacros to determine whether it should exit full screen mode before the
  // session is locked. |callback| will be invoked to signal readiness for
  // session lock.
  void MaybeExitFullscreenBeforeLock(base::OnceClosure callback);

 private:
  void MaybeShowNotification();

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

  const raw_ptr<const SessionControllerImpl> session_controller_;

  std::unique_ptr<FullscreenNotificationBubble> bubble_;

  std::unique_ptr<chromeos::KeepFullscreenForUrlChecker>
      keep_fullscreen_checker_;

  // Whether the screen brightness is low enough to make display dark.
  bool device_in_dark_ = false;
};

}  // namespace ash

#endif  // ASH_SESSION_FULLSCREEN_CONTROLLER_H_
