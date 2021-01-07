// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/video_file_handler.h"
#include "ash/public/cpp/capture_mode_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/services/recording/public/mojom/recording_service.mojom.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace base {
class FilePath;
class Time;
class SequencedTaskRunner;
}  // namespace base

namespace ash {

class CaptureModeSession;
class VideoRecordingWatcher;

// Controls starting and ending a Capture Mode session and its behavior.
class ASH_EXPORT CaptureModeController
    : public recording::mojom::RecordingServiceClient,
      public SessionObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  explicit CaptureModeController(std::unique_ptr<CaptureModeDelegate> delegate);
  CaptureModeController(const CaptureModeController&) = delete;
  CaptureModeController& operator=(const CaptureModeController&) = delete;
  ~CaptureModeController() override;

  // Convenience function to get the controller instance, which is created and
  // owned by Shell.
  static CaptureModeController* Get();

  CaptureModeType type() const { return type_; }
  CaptureModeSource source() const { return source_; }
  CaptureModeSession* capture_mode_session() const {
    return capture_mode_session_.get();
  }
  gfx::Rect user_capture_region() const { return user_capture_region_; }
  bool is_recording_in_progress() const { return is_recording_in_progress_; }

  // Returns true if a capture mode session is currently active.
  bool IsActive() const { return !!capture_mode_session_; }

  // Sets the capture source/type, which will be applied to an ongoing capture
  // session (if any), or to a future capture session when Start() is called.
  void SetSource(CaptureModeSource source);
  void SetType(CaptureModeType type);

  // Starts a new capture session with the most-recently used |type_| and
  // |source_|. Also records what |entry_type| that started capture mode.
  void Start(CaptureModeEntryType entry_type);

  // Stops an existing capture session.
  void Stop();

  // Sets the user capture region. If it's non-empty and changed by the user,
  // update |last_capture_region_update_time_|.
  void SetUserCaptureRegion(const gfx::Rect& region, bool by_user);

  // Full screen capture for each available display if no restricted
  // content exists on that display, each capture is saved as an individual
  // file. Note: this won't start a capture mode session.
  void CaptureScreenshotsOfAllDisplays();

  // Called only while a capture session is in progress to perform the actual
  // capture depending on the current selected |source_| and |type_|, and ends
  // the capture session.
  void PerformCapture();

  void EndVideoRecording(EndRecordingReason reason);

  // Called when the feedback button on the capture bar is pressed.
  void OpenFeedbackDialog();

  // recording::mojom::RecordingServiceClient:
  void OnMuxerOutput(const std::string& chunk) override;
  void OnRecordingEnded(bool success) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnChromeTerminating() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;

  // Skips the 3-second count down, and IsCaptureAllowed() checks, and starts
  // video recording right away for testing purposes.
  void StartVideoRecordingImmediatelyForTesting();

 private:
  friend class CaptureModeTestApi;

  // Used by user session change, and suspend events to end the capture mode
  // session if it's active, or stop the video recording if one is in progress.
  void EndSessionOrRecording(EndRecordingReason reason);

  // Returns the capture parameters for the capture operation that is about to
  // be performed (i.e. the window to be captured, and the capture bounds). If
  // nothing is to be captured (e.g. when there's no window selected in a
  // kWindow source, or no region is selected in a kRegion source), then a
  // base::nullopt is returned.
  struct CaptureParams {
    aura::Window* window = nullptr;
    // The capture bounds, either in root coordinates (in kFullscreen or kRegion
    // capture sources), or window-local coordinates (in a kWindow capture
    // source).
    gfx::Rect bounds;
  };
  base::Optional<CaptureParams> GetCaptureParams() const;

  // Launches the mojo service that handles audio and video recording, and
  // begins recording according to the given |capture_params|.
  void LaunchRecordingServiceAndStartRecording(
      const CaptureParams& capture_params);

  // Called back when the mojo pipe to the recording service gets disconnected.
  void OnRecordingServiceDisconnected();

  // Returns true if doing a screen capture is currently allowed, false
  // otherwise.
  bool IsCaptureAllowed(const CaptureParams& capture_params) const;

  // Called to terminate |is_recording_in_progress_|, the stop-recording shelf
  // pod button, and the |video_recording_watcher_| when recording ends.
  void TerminateRecordingUiElements();

  // The below functions start the actual image/video capture. They expect that
  // the capture session is still active when called, so they can retrieve the
  // capture parameters they need. They will end the sessions themselves.
  // They should never be called if IsCaptureAllowed() returns false.
  void CaptureImage(const CaptureParams& capture_params,
                    const base::FilePath& path);
  void CaptureVideo(const CaptureParams& capture_params);

  // Called back when an image has been captured to trigger an attempt to save
  // the image as a file. |timestamp| is the time at which the capture was
  // triggered, |png_bytes| is the buffer containing the captured image in a
  // PNG format.
  void OnImageCaptured(const base::FilePath& path,
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

  // Called on the UI thread, when |video_file_handler_| finishes a video file
  // IO operation. If an IO failure occurs, i.e. |success| is false, video
  // recording should not continue.
  void OnVideoFileStatus(bool success);

  // Called back when the |video_file_handler_| flushes the remaining cached
  // video chunks in its buffer. Called on the UI thread.
  void OnVideoFileSaved(bool success);

  // Shows a preview notification of the newly taken screenshot or screen
  // recording.
  void ShowPreviewNotification(const base::FilePath& screen_capture_path,
                               const gfx::Image& preview_image,
                               const CaptureModeType type);
  void HandleNotificationClicked(const base::FilePath& screen_capture_path,
                                 const CaptureModeType type,
                                 base::Optional<int> button_index);

  // Builds a path for a file of an image screenshot, or a video screen
  // recording, builds with display index if there are
  // multiple displays.
  base::FilePath BuildImagePath() const;
  base::FilePath BuildVideoPath() const;
  base::FilePath BuildImagePathForDisplay(int display_index) const;
  // Used by the above three functions by providing the corresponding file name
  // |format_string| to a capture type (image or video). The returned file path
  // excludes the file extension. The above functions are responsible for adding
  // it.
  base::FilePath BuildPathNoExtension(const char* const format_string,
                                      base::Time timestamp) const;

  // Records the number of screenshots taken.
  void RecordAndResetScreenshotsTakenInLastDay();
  void RecordAndResetScreenshotsTakenInLastWeek();

  // Records the number of consecutive screenshots taken within 5s of each
  // other.
  void RecordAndResetConsecutiveScreenshots();

  // Called when the video record 3-seconds count down finishes.
  void OnVideoRecordCountDownFinished();

  // Called to interrupt the ongoing video recording because it's not anymore
  // allowed to be captured.
  void InterruptVideoRecording();

  // Called back by |video_file_handler_| when it detects a low disk space
  // condition. In this case we end the video recording to avoid consuming too
  // much space, and we make sure the video preview notification shows a message
  // explaining why the recording ended.
  void OnLowDiskSpace();

  std::unique_ptr<CaptureModeDelegate> delegate_;

  CaptureModeType type_ = CaptureModeType::kImage;
  CaptureModeSource source_ = CaptureModeSource::kRegion;

  // A blocking task runner for file IO operations.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  mojo::Remote<recording::mojom::RecordingService> recording_service_remote_;
  mojo::Receiver<recording::mojom::RecordingServiceClient>
      recording_service_client_receiver_;

  // Callback bound to OnVideoFileStatus() that is triggered repeatedly by
  // |video_file_handler_| to tell us about the status of video file IO
  // operations, so we can end video recording if a failure occurs.
  base::RepeatingCallback<void(bool success)> on_video_file_status_;

  // This is the file path of the video file currently being recorded. It is
  // empty when no video recording is in progress.
  base::FilePath current_video_file_path_;

  // Handles the file IO operations of the video file. This enforces doing all
  // video file related operations on the |blocking_task_runner_|.
  base::SequenceBound<VideoFileHandler> video_file_handler_;

  // We remember the user selected capture region when the source is |kRegion|
  // between sessions. Initially, this value is empty at which point we display
  // a message to the user instructing them to start selecting a region.
  gfx::Rect user_capture_region_;

  std::unique_ptr<CaptureModeSession> capture_mode_session_;

  // Whether the service should record audio.
  bool enable_audio_recording_ = true;

  // True when video recording is in progress.
  bool is_recording_in_progress_ = false;

  // If true, the 3-second countdown UI will be skipped, and video recording
  // will start immediately.
  bool skip_count_down_ui_ = false;

  // True if while writing the video chunks by |video_file_handler_| we detected
  // a low disk space. This value is used only to determine the message shown to
  // the user in the video preview notification to explain why the recording was
  // ended, and is then reset back to false.
  bool low_disk_space_threshold_reached_ = false;

  // Watches events that lead to ending video recording.
  std::unique_ptr<VideoRecordingWatcher> video_recording_watcher_;

  // If set, it will be called when either an image or video file is saved.
  base::OnceCallback<void(const base::FilePath&)> on_file_saved_callback_;

  // Timers used to schedule recording of the number of screenshots taken.
  base::RepeatingTimer num_screenshots_taken_in_last_day_scheduler_;
  base::RepeatingTimer num_screenshots_taken_in_last_week_scheduler_;

  // Counters used to track the number of screenshots taken. These values are
  // not persisted across crashes, restarts or sessions so they only provide a
  // rough approximation.
  int num_screenshots_taken_in_last_day_ = 0;
  int num_screenshots_taken_in_last_week_ = 0;

  // Counter used to track the number of consecutive screenshots taken.
  int num_consecutive_screenshots_ = 0;
  base::DelayTimer num_consecutive_screenshots_scheduler_;

  // The time when OnVideoRecordCountDownFinished is called and video has
  // started recording. It is used when video has finished recording for metrics
  // collection.
  base::TimeTicks recording_start_time_;

  // The last time the user sets a non-empty capture region. It will be used to
  // clear the user capture region from previous capture sessions if 8+ minutes
  // has passed since the last time the user changes the capture region when the
  // new capture session starts .
  base::TimeTicks last_capture_region_update_time_;

  base::WeakPtrFactory<CaptureModeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
