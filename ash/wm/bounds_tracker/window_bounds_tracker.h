// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
#define ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_

namespace ash {

// Tracks the scenarios that need window bounds remapping and restoration.
// Window bounds remapping will be needed if the window being moved to a new
// display configuration without user assigned bounds. While restoration will be
// applied if the window is being moved back to its original display
// configuration. E.g., remapping the window if its host display being removed
// and restoring it if reconnecting the display. Note:
// `PersistentWindowController` will be disabled with this one enabled.
class WindowBoundsTracker {
 public:
  WindowBoundsTracker();
  WindowBoundsTracker(const WindowBoundsTracker&) = delete;
  WindowBoundsTracker& operator=(const WindowBoundsTracker&) = delete;
  ~WindowBoundsTracker();
};

}  // namespace ash

#endif  // ASH_WM_BOUNDS_TRACKER_WINDOW_BOUNDS_TRACKER_H_
