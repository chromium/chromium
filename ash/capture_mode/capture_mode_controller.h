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
#include "ash/public/cpp/capture_mode_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/services/recording/public/mojom/recording_service.mojom.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  bool enable_audio_recording() const { return enable_audio_recording_; }
  bool is_recording_in_progress() const { return is_recording_in_progress_; }

  // Returns true if a capture mode session is currently active. If you only
  // need to call this method, but don't need the rest of the controller, use
  // capture_mode_util::IsCaptureModeActive().
  bool IsActive() const;

  // Sets the capture source/type, which will be applied to an ongoing capture
  // session (if any), or to a future capture session when Start() is called.
  void SetSource(CaptureModeSource source);
  void SetType(CaptureModeType type);

  // Sets the audio recording flag, which will be applied to any future
  // recordings (cannot be set mid recording), or to a future capture mode
  // session when Start() is called.
  void EnableAudioRecording(bool enable_audio_recording);

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

  // Sets the |protection_mask| that is currently set on the given |window|. If
  // the |protection_mask| is |display::CONTENT_PROTECTION_METHOD_NONE|, then
  // the window will no longer be tracked.
  // Note that content protection (a.k.a. HDCP (High-bandwidth Digital Content
  // Protection)) is different from DLP (Data Leak Prevention). The latter is
  // enforced by admins and applies to both image and video capture, whereas
  // the former is enforced by apps and content providers and is applied only to
  // video capture.
  void SetWindowProtectionMask(aura::Window* window, uint32_t protection_mask);

  // If a video recording is in progress, it will end if so required by content
  // protection.
  void RefreshContentProtection();

  // recording::mojom::RecordingServiceClient:
  void OnRecordingEnded(recording::mojom::RecordingStatus status,
                        const gfx::ImageSkia& thumbnail) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnChromeTerminating() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;

  // Skips the 3-second count down, and IsCaptureAllowed() checks, and starts
  // video recording right away for testing purposes.
  void StartVideoRecordingImmediatelyForTesting();

  CaptureModeDelegate* delegate_for_testing() const { return delegate_.get(); }
  VideoRecordingWatcher* video_recording_watcher_for_testing() const {
    return video_recording_watcher_.get();
  }

 private:
  friend class CaptureModeTestApi;
  friend class VideoRecordingWatcher;

  // Called by |video_recording_watcher_| when the display on which recording is
  // happening changes its bounds such as on display rotation or device scale
  // factor changes. In this case we push the new |root_size| in DIPs, and the
  // |device_scale_factor| to the recording service so that it can update the
  // video size. Note that we do this only when recording a window or a partial
  // region. When recording a fullscreen, the capturer can handle these changes
  // and would center and letter-box the video frames within the requested size.
  void PushNewRootSizeToRecordingService(const gfx::Size& root_size,
                                         float device_scale_factor);

  // Called by |video_recording_watcher_| to inform us that the |window| being
  // recorded (i.e. |is_recording_in_progress_| is true) is about to move to a
  // |new_root|. This is needed so we can inform the recording service of this
  // change so that it can switch its capture target to the new root's frame
  // sink.
  void OnRecordedWindowChangingRoot(aura::Window* window,
                                    aura::Window* new_root);

  // Called by |video_recording_watcher_| to inform us that the size of the
  // |window| being recorded was changed to |new_size| in DIPs. This is pushed
  // to the recording service in order to update the video dimensions.
  void OnRecordedWindowSizeChanged(const gfx::Size& new_size);

  // Returns true if screen recording needs to be blocked due to protected
  // content. |window| is the window being recorded or desired to be recorded.
  bool ShouldBlockRecordingForContentProtection(aura::Window* window) const;

  // Used by user session change, and suspend events to end the capture mode
  // session if it's active, or stop the video recording if one is in progress.
  void EndSessionOrRecording(EndRecordingReason reason);

  // Returns the capture parameters for the capture operation that is about to
  // be performed (i.e. the window to be captured, and the capture bounds). If
  // nothing is to be captured (e.g. when there's no window selected in a
  // kWindow source, or no region is selected in a kRegion source), then a
  // absl::nullopt is returned.
  struct CaptureParams {
    aura::Window* window = nullptr;
    // The capture bounds, either in root coordinates (in kFullscreen or kRegion
    // capture sources), or window-local coordinates (in a kWindow capture
    // source).
    gfx::Rect bounds;
  };
  absl::optional<CaptureParams> GetCaptureParams() const;

  // Launches the mojo service that handles audio and video recording, and
  // begins recording according to the given |capture_params|. It creates an
  // overlay on the video capturer so that can be used to record the mouse
  // cursor. It gives the pending receiver end to that overlay on Viz, and the
  // other end should be owned by the |video_recording_watcher_|.
  void LaunchRecordingServiceAndStartRecording(
      const CaptureParams& capture_params,
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay>
          cursor_overlay);

  // Called back when the mojo pipe to the recording service gets disconnected.
  void OnRecordingServiceDisconnected();

  // Returns whether doing a screen capture is currently allowed by enterprise
  // policies and a reason otherwise.
  // ShouldBlockRecordingForContentProtection() should be used for HDCP checks.
  CaptureAllowance IsCaptureAllowedByEnterprisePolicies(
      const CaptureParams& capture_params) const;

  // Terminates the recording service process, closes any recording-related UI
  // elements (only if |success| is false as this indicates that recording was
  // not ended normally by calling EndVideoRecording()), and shows the video
  // file notification with the given |thumbnail|.
  void FinalizeRecording(bool success, const gfx::ImageSkia& thumbnail);

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
  // triggered. |was_cursor_originally_blocked| is whether the cursor was
  // blocked at the time the screenshot capture request was made. |png_bytes| is
  // the buffer containing the captured image in a PNG format.
  void OnImageCaptured(const base::FilePath& path,
                       bool was_cursor_originally_blocked,
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

  // Called back when the |video_file_handler_| flushes the remaining cached
  // video chunks in its buffer. Called on the UI thread. |video_thumbnail| is
  // an RGB image provided by the recording service that can be used as a
  // thumbnail of the video in the notification.
  void OnVideoFileSaved(const gfx::ImageSkia& video_thumbnail, bool success);

  // Shows a preview notification of the newly taken screenshot or screen
  // recording.
  void ShowPreviewNotification(const base::FilePath& screen_capture_path,
                               const gfx::Image& preview_image,
                               const CaptureModeType type);
  void HandleNotificationClicked(const base::FilePath& screen_capture_path,
                                 const CaptureModeType type,
                                 absl::optional<int> button_index);

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

  std::unique_ptr<CaptureModeDelegate> delegate_;

  CaptureModeType type_ = CaptureModeType::kImage;
  CaptureModeSource source_ = CaptureModeSource::kRegion;

  // A blocking task runner for file IO operations.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  mojo::Remote<recording::mojom::RecordingService> recording_service_remote_;
  mojo::Receiver<recording::mojom::RecordingServiceClient>
      recording_service_client_receiver_;

  // This is the file path of the video file currently being recorded. It is
  // empty when no video recording is in progress.
  base::FilePath current_video_file_path_;

  // We remember the user selected capture region when the source is |kRegion|
  // between sessions. Initially, this value is empty at which point we display
  // a message to the user instructing them to start selecting a region.
  gfx::Rect user_capture_region_;

  std::unique_ptr<CaptureModeSession> capture_mode_session_;

  // Remember the user selected audio preference of whether to record audio or
  // not for a video, between sessions. Initially, this value is set to false,
  // ensuring that this is an opt-in feature.
  bool enable_audio_recording_ = false;

  // True when video recording is in progress.
  bool is_recording_in_progress_ = false;

  // If true, the 3-second countdown UI will be skipped, and video recording
  // will start immediately.
  bool skip_count_down_ui_ = false;

  // True only if the recording service detects a |kLowDiskSpace| condition
  // while writing the video file to the file system. This value is used only to
  // determine the message shown to the user in the video preview notification
  // to explain why the recording was ended, and is then reset back to false.
  bool low_disk_space_threshold_reached_ = false;

  // Watches events that lead to ending video recording.
  std::unique_ptr<VideoRecordingWatcher> video_recording_watcher_;

  // Tracks the windows that currently have content protection enabled, so that
  // we prevent them from being video recorded. Each window is mapped to its
  // currently-set protection_mask. Windows in this map are only the ones that
  // have protection masks other than |display::CONTENT_PROTECTION_METHOD_NONE|.
  base::flat_map<aura::Window*, /*protection_mask*/ uint32_t>
      protected_windows_;

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
