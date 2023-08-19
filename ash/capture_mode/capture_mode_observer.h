// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_OBSERVER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_OBSERVER_H_

#include "base/observer_list_types.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Defines an interface that's used to observe changes in capture mode.
class CaptureModeObserver : public base::CheckedObserver {
 public:
  // Called to notify with the state of a video recording. `current_root` is the
  // root window, which is either being captured itself or a descendant of it.
  virtual void OnRecordingStarted(aura::Window* current_root) = 0;
  virtual void OnRecordingEnded() = 0;

  // Called when the status of the video is confirmed. DLP can potentially show
  // users a dialog to warn them about restricted contents in the video, and
  // recommending that they delete the file. In this case,
  // `user_deleted_video_file` will be true. `thumbnail` contains an image
  // representation of the video, which can be empty if there were errors during
  // recording.
  virtual void OnVideoFileFinalized(bool user_deleted_video_file,
                                    const gfx::ImageSkia& thumbnail) = 0;

  // Called when the window being recorded is moved from one display to another.
  virtual void OnRecordedWindowChangingRoot(aura::Window* new_root) = 0;

  // Called to notify us that a recording session was aborted (i.e. recording
  // was never started) due to e.g. user cancellation, an error, or a DLP/HDCP
  // restriction.
  virtual void OnRecordingStartAborted() = 0;

 protected:
  ~CaptureModeObserver() override = default;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_OBSERVER_H_
