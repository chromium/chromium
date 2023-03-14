// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_OBSERVER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_OBSERVER_H_

#include <string>

#include "base/functional/callback.h"

namespace ash {

class ScreenSecurityObserver {
 public:
  // Called when the screen starts being accessed (i.e. screen sharing, screen
  // capture). Note that this function will not be called during system screen
  // capture (please consult the owners of //ash/capture_mode for this) and
  // during screen sharing via remoting (see functions below for this).
  // `stop_callback` is the callback to stop the stream.
  // `source_callback` is the callback to change the desktop capture source.
  virtual void OnScreenAccessStart(
      base::OnceClosure stop_callback,
      const base::RepeatingClosure& source_callback,
      const std::u16string& access_app_name) {}

  // Called when the screen is no longer being accessed.
  virtual void OnScreenAccessStop() {}

  // Called when screen share via remoting is started.
  virtual void OnRemotingScreenShareStart(base::OnceClosure stop_callback) {}

  // Called when screen share via remoting is stopped.
  virtual void OnRemotingScreenShareStop() {}

 protected:
  virtual ~ScreenSecurityObserver() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_OBSERVER_H_
