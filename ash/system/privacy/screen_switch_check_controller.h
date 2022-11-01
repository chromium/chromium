// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_SWITCH_CHECK_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_SWITCH_CHECK_CONTROLLER_H_

#include "ash/system/privacy/screen_capture_observer.h"
#include "ash/system/privacy/screen_share_observer.h"

namespace ash {

// Controller of a dialog that confirms the user wants to stop screen share/cast
// on user profile switching.
class ScreenSwitchCheckController : public ScreenCaptureObserver,
                                    public ScreenShareObserver {
 public:
  ScreenSwitchCheckController();

  ScreenSwitchCheckController(const ScreenSwitchCheckController&) = delete;
  ScreenSwitchCheckController& operator=(const ScreenSwitchCheckController&) =
      delete;

  ~ScreenSwitchCheckController() override;

  // Determines if it's ok to switch away from the currently active user. Screen
  // casting may block this (or at least throw up a confirmation dialog). Calls
  // |callback| with the result.
  void CanSwitchAwayFromActiveUser(base::OnceCallback<void(bool)> callback);

 private:
  // ScreenCaptureObserver:
  void OnScreenCaptureStart(
      base::OnceClosure stop_callback,
      const base::RepeatingClosure& source_callback,
      const std::u16string& screen_capture_status) override;
  void OnScreenCaptureStop() override;

  // ScreenShareObserver:
  void OnScreenShareStart(base::OnceClosure stop_callback,
                          const std::u16string& helper_name) override;
  void OnScreenShareStop() override;

  bool has_capture_ = false;
  bool has_share_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_SWITCH_CHECK_CONTROLLER_H_
