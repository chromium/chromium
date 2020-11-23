// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_TEST_API_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_TEST_API_H_

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class CaptureModeController;

// Exposes a very limited API for browser tests, and possible autotest private
// APIs to interact with the capture mode feature.
class ASH_EXPORT CaptureModeTestApi {
 public:
  CaptureModeTestApi();
  CaptureModeTestApi(const CaptureModeTestApi&) = delete;
  CaptureModeTestApi& operator=(const CaptureModeTestApi&) = delete;
  ~CaptureModeTestApi() = default;

  // APIs to start capture mode from the three possible sources (fullscreen,
  // window, or region). If |for_video| is true, a video will be recorded from
  // the chosen source once capture begins, otherwise an image will be
  // captured.
  void StartForFullscreen(bool for_video);
  void StartForWindow(bool for_video);
  void StartForRegion(bool for_video);

  // Sets the user selected region for partial screen capture.
  void SetUserSelectedRegion(const gfx::Rect& region);

  // Can only be called after one of the above APIs starts capture mode.
  // Depending on how capture mode was started from the above APIs, this will
  // perform the capture of either an image or a video from the chosen source.
  // Note that for video capture, this skips the 3-second count down UIs, and
  // starts video recording immediately.
  void PerformCapture();

  // Stops the video recording. Can only be called if a video recording was
  // in progress.
  void StopVideoRecording();

  // Sets a callback that will be triggered once the captured file (of an
  // image or a video) is saved, providing its path. It will never be
  // triggered if capture failed to save a file.
  using OnFileSavedCallback = base::OnceCallback<void(const base::FilePath&)>;
  void SetOnCaptureFileSavedCallback(OnFileSavedCallback callback);

  // Sets whether or not audio will be recorded when capturing a video. Should
  // only be called before recording starts, otherwise it has no effect.
  void SetAudioRecordingEnabled(bool enabled);

  // Flushes the recording service pipe synchronously. Can only be called while
  // recording is in progress.
  void FlushRecordingServiceForTesting();

 private:
  // Sets the capture mode type to a video capture if |for_video| is true, or
  // image capture otherwise.
  void SetType(bool for_video);

  CaptureModeController* const controller_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_TEST_API_H_
