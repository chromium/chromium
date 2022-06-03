// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CAMERA_DETECTOR_H_
#define CHROME_BROWSER_ASH_CAMERA_DETECTOR_H_

#include "base/callback.h"

namespace ash {

// Class used to check for camera presence.
class CameraDetector {
 public:
  enum CameraPresence {
    kCameraPresenceUnknown = 0,
    kCameraAbsent,
    kCameraPresent
  };

  CameraDetector(const CameraDetector&) = delete;
  CameraDetector& operator=(const CameraDetector&) = delete;

  // Returns result of the last presence check. If no check has been performed
  // yet, returns |kCameraPresenceUnknown|.
  static CameraPresence camera_presence() {
    return camera_presence_;
  }

  // Checks asynchronously for camera device presence. Only one
  // presence check can be running at a time. Calls |check_done|
  // on current thread when the check has been completed.
  static void StartPresenceCheck(base::OnceClosure check_done);

 private:
  // Called back on UI thread after check has been performed.
  static void OnPresenceCheckDone(base::OnceClosure callback, bool present);

  // Checks for camera presence. Runs on a thread that allows I/O.
  static bool CheckPresence();

  // Result of the last presence check.
  static CameraPresence camera_presence_;

  static bool presence_check_in_progress_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CAMERA_DETECTOR_H_
