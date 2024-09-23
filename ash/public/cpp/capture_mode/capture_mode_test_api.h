// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_TEST_API_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
class VideoFrame;
}  // namespace media

namespace views {
class Widget;
}  // namespace views

namespace ash {

class CaptureModeController;
class AnnotationsOverlayController;

// Exposes a very limited API for browser tests, and possible autotest private
// APIs to interact with the capture mode feature.
class ASH_EXPORT CaptureModeTestApi {
 public:
  CaptureModeTestApi();
  CaptureModeTestApi(const CaptureModeTestApi&) = delete;
  CaptureModeTestApi& operator=(const CaptureModeTestApi&) = delete;
  ~CaptureModeTestApi() = default;

  // APIs to start capture mode from the three possible sources (fullscreen,
  // window, or region). If `for_video` is true, a video will be recorded from
  // the chosen source once capture begins, otherwise an image will be
  // captured.
  void StartForFullscreen(bool for_video);
  void StartForWindow(bool for_video);
  void StartForRegion(bool for_video);

  // API to set the capture mode source with given `source`.
  void SetCaptureModeSource(CaptureModeSource source);

  // Sets the `recording_type` to use for video captures.
  void SetRecordingType(RecordingType recording_type);

  // Returns true if a capture mode session is currently active.
  bool IsSessionActive() const;

  // Sets the user selected region for partial screen capture.
  void SetUserSelectedRegion(const gfx::Rect& region);

  // Can only be called after one of the above APIs starts capture mode.
  // Depending on how capture mode was started from the above APIs, this will
  // perform the capture of either an image or a video from the chosen source.
  // Note that for video capture, this skips the 3-second count down UIs, and
  // starts video recording immediately.
  void PerformCapture(bool skip_count_down = true);

  // Returns true if there is a video recording currently in progress.
  bool IsVideoRecordingInProgress() const;

  // Returns true if capture mode is waiting for a reply from the DLP manager to
  // check content restrictions.
  bool IsPendingDlpCheck() const;

  // Returns true if there's an active session in a waiting state for the DLP
  // confirmation.
  bool IsSessionWaitingForDlpConfirmation() const;

  // Returns true if the 3-second countdown animation is in progress.
  bool IsInCountDownAnimation() const;

  // Sets a callback that will be triggered once the video recording is started.
  void SetOnVideoRecordingStartedCallback(base::OnceClosure callback);

  // Sets a callback that will be triggered once the image is captured and
  // encoded as JPEG bytes.
  void SetOnImageCapturedForSearchCallback(base::OnceClosure callback);

  // Stops the video recording. Can only be called if a video recording was
  // in progress.
  void StopVideoRecording();

  // Sets a callback that will be triggered once the captured file (of an image
  // or a video) is saved, providing its path. It will never be triggered if
  // capture failed to save a file.
  using OnFileSavedCallback = base::OnceCallback<void(const base::FilePath&)>;
  void SetOnCaptureFileSavedCallback(OnFileSavedCallback callback);

  // Sets a callback that will be triggered once the captured file (of an image
  // or a video) is deleted as a result of user action at the end of the video
  // (e.g. clicking the "Delete" button in the notification, or in the DLP
  // warning dialog). The callback is provided with the file path, and whether
  // the deletion was successful or not.
  using OnFileDeletedCallback =
      base::OnceCallback<void(const base::FilePath& path,
                              bool delete_successful)>;
  void SetOnCaptureFileDeletedCallback(OnFileDeletedCallback callback);

  // Sets a callback that will be triggered once the video record countdown is
  // finished.
  void SetOnVideoRecordCountdownFinishedCallback(base::OnceClosure callback);

  // Sets the audio recording mode when capturing a video. Should only be called
  // before recording starts, otherwise it has no effect.
  void SetAudioRecordingMode(AudioRecordingMode mode);

  // Returns the effective mode of audio recording which takes into account the
  // `AudioCaptureAllowed` policy.
  AudioRecordingMode GetEffectiveAudioRecordingMode() const;

  // Flushes the recording service pipe synchronously. Can only be called while
  // recording is in progress.
  void FlushRecordingServiceForTesting();

  // APIs to reset both the recording service remote, and its client receiver in
  // order to test that these events are correctly handled.
  void ResetRecordingServiceRemote();
  void ResetRecordingServiceClientReceiver();

  // Returns the `AnnotationsOverlayController` which hosts the overlay widget.
  // Can only be called while recording is in progress for a Projector session.
  AnnotationsOverlayController* GetAnnotationsOverlayController();

  // Simulates the flow taken by users to open the folder selection dialog from
  // the settings menu, and waits until this dialog gets added.
  void SimulateOpeningFolderSelectionDialog();

  // Returns a pointer to the folder selection dialog window or nullptr if no
  // such window exists.
  aura::Window* GetFolderSelectionDialogWindow();

  // If `value` is true, the `kGpuMemoryBuffer` type will be requested even when
  // running on linux-chromeos.
  void SetForceUseGpuMemoryBufferForCameraFrames(bool value);

  // Returns the number of cameras currently connected.
  size_t GetNumberOfAvailableCameras() const;

  // Sets the camera at `index` of
  // `CaptureModeCameraController::available_cameras()` as the selected camera.
  void SelectCameraAtIndex(size_t index);

  // Unselects the currently selected camera (if any).
  void TurnCameraOff();

  using CameraVideoFrameCallback =
      base::OnceCallback<void(scoped_refptr<media::VideoFrame>)>;
  void SetOnCameraVideoFrameRendered(CameraVideoFrameCallback callback);

  // Returns the camera preview widget if exists and nullptr otherwise.
  views::Widget* GetCameraPreviewWidget();

  CaptureModeBehavior* GetBehavior(BehaviorType behavior_type);

 private:
  // Sets the capture mode type to a video capture if |for_video| is true, or
  // image capture otherwise.
  void SetType(bool for_video);

  const raw_ptr<CaptureModeController> controller_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_TEST_API_H_
