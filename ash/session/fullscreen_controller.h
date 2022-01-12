// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_FULLSCREEN_CONTROLLER_H_
#define ASH_SESSION_FULLSCREEN_CONTROLLER_H_

#include <memory>

#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"

class PrefRegistrySimple;

namespace ash {

class SessionControllerImpl;
class FullscreenNotificationBubble;

class FullscreenController : public chromeos::PowerManagerClient::Observer {
 public:
  explicit FullscreenController(SessionControllerImpl* session_controller);
  FullscreenController(const FullscreenController&) = delete;
  FullscreenController& operator=(const FullscreenController&) = delete;

  ~FullscreenController() override;

  static void MaybeExitFullscreen();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static bool ShouldExitFullscreenBeforeLock();

 private:
  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;

  void MaybeShowNotification();

  const SessionControllerImpl* const session_controller_;

  std::unique_ptr<FullscreenNotificationBubble> bubble_;

  // Whether the screen brightness is low enough to make display dark.
  bool device_in_dark_ = false;
};

}  // namespace ash

#endif  // ASH_SESSION_FULLSCREEN_CONTROLLER_H_
