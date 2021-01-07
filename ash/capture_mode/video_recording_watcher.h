// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_
#define ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_

#include "ui/aura/scoped_window_capture_request.h"
#include "ui/aura/window_observer.h"

namespace ash {

class CaptureModeController;

// An instance of this class is created while video recording is in progress to
// watch for events that end video recording, such as a window being recorded
// gets closed or moved between displays, or a display being fullscreen-recorded
// gets disconnected.
// TODO(https://crbug.com/1145003): Use this to paint a border around the area
// being recorded while recording is in progress.
class VideoRecordingWatcher : public aura::WindowObserver {
 public:
  VideoRecordingWatcher(CaptureModeController* controller,
                        aura::Window* window_being_recorded);
  ~VideoRecordingWatcher() override;

  aura::Window* window_being_recorded() const { return window_being_recorded_; }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

 private:
  CaptureModeController* const controller_;
  aura::Window* const window_being_recorded_;

  // If |window_being_recorded_| is not a root window, we must make a request to
  // make it capturable by the |FrameSinkVideoCapturer|.
  aura::ScopedWindowCaptureRequest non_root_window_capture_request_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_VIDEO_RECORDING_WATCHER_H_
