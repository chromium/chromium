// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SCREEN_SECURITY_SCREEN_CAPTURE_OBSERVER_H_
#define ASH_SYSTEM_SCREEN_SECURITY_SCREEN_CAPTURE_OBSERVER_H_

#include <string>

#include "base/callback.h"

namespace ash {

class ScreenCaptureObserver {
 public:
  // Called when screen capture is started.
  // |stop_callback| is a callback to stop the stream.
  // |source_callback| is a callback to change the desktop capture source.
  // These must be base::RepeatingCallbacks so that they can be passed to all
  // observers.
  virtual void OnScreenCaptureStart(
      const base::RepeatingClosure& stop_callback,
      const base::RepeatingClosure& source_callback,
      const std::u16string& screen_capture_status) = 0;

  // Called when screen capture is stopped.
  virtual void OnScreenCaptureStop() = 0;

 protected:
  virtual ~ScreenCaptureObserver() {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_SCREEN_SECURITY_SCREEN_CAPTURE_OBSERVER_H_
