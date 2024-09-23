// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_SETTINGS_EVENT_HANDLER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_SETTINGS_EVENT_HANDLER_H_

#include <vector>

namespace ash {
struct FaceGazeGestureInfo;
}  // namespace ash

namespace ash {

// Helper class for handling events from FaceGaze and sending them to the
// Settings.
class FaceGazeSettingsEventHandler {
 public:
  // Sends gesture info to the Settings in ChromeOS.
  virtual void HandleSendGestureInfoToSettings(
      const std::vector<ash::FaceGazeGestureInfo>& gesture_info) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_SETTINGS_EVENT_HANDLER_H_
