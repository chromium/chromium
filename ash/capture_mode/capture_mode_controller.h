// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/public/cpp/capture_mode_delegate.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

namespace ash {

class CaptureModeSession;

// Controls starting and ending a Capture Mode session and its behavior.
class ASH_EXPORT CaptureModeController {
 public:
  explicit CaptureModeController(std::unique_ptr<CaptureModeDelegate> delegate);
  CaptureModeController(const CaptureModeController&) = delete;
  CaptureModeController& operator=(const CaptureModeController&) = delete;
  ~CaptureModeController();

  // Convenience function to get the controller instance, which is created and
  // owned by Shell.
  static CaptureModeController* Get();

  CaptureModeType type() const { return type_; }
  CaptureModeSource source() const { return source_; }
  CaptureModeSession* capture_mode_session() const {
    return capture_mode_session_.get();
  }
  gfx::Rect user_capture_region() const { return user_capture_region_; }
  void set_user_capture_region(const gfx::Rect& region) {
    user_capture_region_ = region;
  }
  bool is_recording_in_progress() const { return is_recording_in_progress_; }

  // Returns true if a capture mode session is currently active.
  bool IsActive() const { return !!capture_mode_session_; }

  // Sets the capture source/type, which will be applied to an ongoing capture
  // session (if any), or to a future capture session when Start() is called.
  void SetSource(CaptureModeSource source);
  void SetType(CaptureModeType type);

  // Starts a new capture session with the most-recently used |type_| and
  // |source_|.
  void Start();

  // Stops an existing capture session.
  void Stop();

  // Called only while a capture session is in progress to perform the actual
  // capture depending on the current selected |source_| and |type_|, and ends
  // the capture session.
  void PerformCapture();

  void EndVideoRecording();

 private:
  // Returns true if doing a screen capture is currently allowed, false
  // otherwise.
  bool IsCaptureAllowed() const;

  // Returns the capture parameters for the capture operation that is about to
  // be performed (i.e. the window to be captured, and the capture bounds). If
  // nothing is to be captured (e.g. when there's no window selected in a
  // kWindow source, or no region is selected in a kRegion source), then a
  // base::nullopt is returned.
  struct CaptureParams {
    aura::Window* window = nullptr;
    // The capture bounds, either in root coordinates (in kFullscreen or kRegion
    // capture sources), or window-local coordinates (in a kWindow capture
    // source). The bounds are never empty when in kImage capture type. However,
    // in kVideo capture type, they're non-empty only in a kRegion capture
    // source, since the recording service needs them to crop the frame.
    gfx::Rect bounds;
  };
  base::Optional<CaptureParams> GetCaptureParams() const;

  // The below functions start the actual image/video capture. They expect that
  // the capture session is still active when called, so they can retrieve the
  // capture parameters they need. They will end the sessions themselves.
  // They should never be called if IsCaptureAllowed() returns false.
  void CaptureImage();
  void CaptureVideo();

  // Called back when an image has been captured to trigger an attempt to save
  // the image as a file. |timestamp| is the time at which the capture was
  // triggered, and |png_bytes| is the buffer containing the captured image in a
  // PNG format.
  void OnImageCaptured(base::Time timestamp,
                       scoped_refptr<base::RefCountedMemory> png_bytes);

  // Called back when an attempt to save the image file has been completed, with
  // |success| indicating whether the attempt succeeded for failed. |png_bytes|
  // is the buffer containing the captured image in a PNG format, which will be
  // used to show a preview of the image in a notification, and save it as a
  // bitmap in the clipboard. If saving was successful, then the image was saved
  // in |path|.
  void OnImageFileSaved(scoped_refptr<base::RefCountedMemory> png_bytes,
                        const base::FilePath& path,
                        bool success);

  // Shows a preview notification of the newly taken screenshot or screen
  // recording.
  void ShowPreviewNotification(const base::FilePath& screen_capture_path,
                               const gfx::Image& preview_image);
  void HandleNotificationClicked(const base::FilePath& screen_capture_path,
                                 base::Optional<int> button_index);

  // Builds a path for a file of an image screenshot, or a video screen
  // recording, which were taken at |timestamp|.
  base::FilePath BuildImagePath(base::Time timestamp) const;
  base::FilePath BuildVideoPath(base::Time timestamp) const;
  // Used by the above two functions by providing the corresponding file name
  // |format_string| to a capture type (image or video).
  base::FilePath BuildPath(const char* const format_string,
                           base::Time timestamp) const;

  // Records the number of screenshots taken.
  void RecordNumberOfScreenshotsTakenInLastDay();
  void RecordNumberOfScreenshotsTakenInLastWeek();

  std::unique_ptr<CaptureModeDelegate> delegate_;

  CaptureModeType type_ = CaptureModeType::kImage;
  CaptureModeSource source_ = CaptureModeSource::kRegion;

  // We remember the user selected capture region when the source is |kRegion|
  // between sessions. Initially, this value is empty at which point we display
  // a message to the user instructing them to start selecting a region.
  gfx::Rect user_capture_region_;

  std::unique_ptr<CaptureModeSession> capture_mode_session_;

  // True when video recording is in progress.
  bool is_recording_in_progress_ = false;

  // Timers used to schedule recording of the number of screenshots taken.
  base::RepeatingTimer num_screenshots_taken_in_last_day_scheduler_;
  base::RepeatingTimer num_screenshots_taken_in_last_week_scheduler_;

  // Counters used to track the number of screenshots taken.
  int num_screenshots_taken_in_last_day_ = 0;
  int num_screenshots_taken_in_last_week_ = 0;

  base::WeakPtrFactory<CaptureModeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
