// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_education_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

class PrefRegistrySimple;

namespace base {
class FilePath;
class Time;
class SequencedTaskRunner;
}  // namespace base

namespace ash {

class CaptureModeBehavior;
class CaptureModeCameraController;
class CaptureModeObserver;
class BaseCaptureModeSession;

// Defines a callback type that will be invoked when an attempt to delete the
// given `path` is completed with the given status `delete_successful`.
using OnFileDeletedCallback =
    base::OnceCallback<void(const base::FilePath& path,
                            bool delete_successful)>;

// Defines a callback type that will be invoked when the status of the capture
// mode session initialization process is determined with the given status of
// `success`.
using OnSessionStartAttemptCallback = base::OnceCallback<void(bool success)>;

// Controls starting and ending a Capture Mode session and its behavior. There
// are various checks that are run when a capture session start is attempted,
// and when a capture operation is performed, to make sure they're allowed. For
// example, checking that policy allows screen capture, and there are no content
// on the screen restricted by DLP (Data Leak Prevention). In the case of video
// recording, HDCP is also checked to ensure no protected content is being
// recorded.
class ASH_EXPORT CaptureModeController
    : public recording::mojom::RecordingServiceClient,
      public recording::mojom::DriveFsQuotaDelegate,
      public SessionObserver,
      public chromeos::PowerManagerClient::Observer,
      public crosapi::mojom::VideoConferenceManagerClient {
 public:
  // Contains info about the folder used for saving the captured images and
  // videos.
  struct CaptureFolder {
    // The absolute path of the folder used for saving the captures.
    base::FilePath path;

    // True if the above `path` is the default "Downloads" folder on the device.
    bool is_default_downloads_folder = false;
  };

  explicit CaptureModeController(std::unique_ptr<CaptureModeDelegate> delegate);
  CaptureModeController(const CaptureModeController&) = delete;
  CaptureModeController& operator=(const CaptureModeController&) = delete;
  ~CaptureModeController() override;

  // Convenience function to get the controller instance, which is created and
  // owned by Shell.
  static CaptureModeController* Get();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  CaptureModeCameraController* camera_controller() {
    return camera_controller_.get();
  }
  CaptureModeType type() const { return type_; }
  CaptureModeSource source() const { return source_; }
  RecordingType recording_type() const { return recording_type_; }

  // Returns the raw audio recording mode, without taking into account the
  // `AudioCaptureAllowed` policy.
  AudioRecordingMode audio_recording_mode() const {
    return audio_recording_mode_;
  }
  BaseCaptureModeSession* capture_mode_session() const {
    return capture_mode_session_.get();
  }
  gfx::Rect user_capture_region() const { return user_capture_region_; }
  bool is_recording_in_progress() const {
    return is_initializing_recording_ ||
           (video_recording_watcher_ &&
            !video_recording_watcher_->is_shutting_down());
  }
  bool can_start_new_recording() const {
    // Even if recording stops, it takes a while for the video/gif file to be
    // fully finalized. Until that happens, a new recording is not allowed to
    // start.
    return current_video_file_path_.empty() && !is_recording_in_progress();
  }
  bool enable_demo_tools() const { return enable_demo_tools_; }

  CaptureModeEducationController* education_controller() {
    return education_controller_.get();
  }

  // Returns true if a capture mode session is currently active. If you only
  // need to call this method, but don't need the rest of the controller, use
  // capture_mode_util::IsCaptureModeActive(). Note that null sessions can still
  // return `true`.
  bool IsActive() const;

  // Returns the effective audio recording mode, taking into account the
  // `AudioCaptureAllowed` policy.
  AudioRecordingMode GetEffectiveAudioRecordingMode() const;

  // Returns true if audio recording is forced disabled by the
  // `AudioCaptureAllowed` policy.
  bool IsAudioCaptureDisabledByPolicy() const;

  // Returns true if the folder to save captures is enforced by
  // `ScreenCaptureLocation` policy and can't be changed.
  bool IsCustomFolderManagedByPolicy() const;

  // Returns true if there's an active video recording that is recording audio.
  bool IsAudioRecordingInProgress() const;

  // Returns true if the camera preview is visible, false otherwise.
  bool IsShowingCameraPreview() const;

  // Returns true if this supports the new behavior provided by
  // `new_entry_type`.
  bool SupportsBehaviorChange(CaptureModeEntryType new_entry_type) const;

  // Sets the capture source/type, and recording type, which will be applied to
  // an ongoing capture session (if any), or to a future capture session when
  // Start() is called.
  void SetSource(CaptureModeSource source);
  void SetType(CaptureModeType type);
  void SetRecordingType(RecordingType recording_type);

  // Sets the audio recording mode, which will be applied to any future
  // recordings (cannot be set mid recording), or to a future capture mode
  // session when Start() is called. The effective enabled state takes into
  // account the `AudioCaptureAllowed` policy.
  void SetAudioRecordingMode(AudioRecordingMode mode);

  // Sets the flag to enable the demo tools feature, which will be applied to
  // any future recordings (cannot be set mid recording), or to a future capture
  // mode session when Start() is called. Currently the demo tools feature is
  // behind the feature flag.
  void EnableDemoTools(bool enable);

  // Starts a new capture session with the most-recently used `type_` and
  // `source_`. Also records what `entry_type` that started capture mode. The
  // `callback` will be invoked when the status of the capture mode session
  // initialization process is determined.
  void Start(CaptureModeEntryType entry_type,
             OnSessionStartAttemptCallback callback = base::DoNothing());

  // Starts a new capture session with a pre-selected window which will be
  // observed throughout the session and can't be altered.
  void StartForGameDashboard(aura::Window* game_window);

  // Starts recording a pre-selected game window as soon as possible without
  // starting a countdown by using a null session. Currently unused.
  void StartRecordingInstantlyForGameDashboard(aura::Window* game_window);

  // Starts a new sunfish session. Currently only invoked via a debug command.
  void StartSunfishSession();

  // Stops an existing capture session.
  void Stop();

  // Called by the `capture_mode_session_` to notify the `observers_` when the
  // capture mode session ends without starting any recording
  void NotifyRecordingStartAborted();

  // Sets the user capture region. If it's non-empty and changed by the user,
  // update |last_capture_region_update_time_|.
  void SetUserCaptureRegion(const gfx::Rect& region, bool by_user);

  // Returns true if we can show a user nudge animation and a toast message to
  // alert users any available new features.
  bool CanShowUserNudge() const;

  // Disables showing the user nudge from now on. Calling the above
  // CanShowUserNudge() will return false for the current active user going
  // forward.
  void DisableUserNudgeForever();

  // Sets whether the currently logged in user selected to use the default
  // "Downloads" folder as the current save location, even while they already
  // have a currently set custom folder. When this setting is true, any
  // currently set custom folder is ignored but not removed.
  // This can only be called when user is logged in.
  void SetUsesDefaultCaptureFolder(bool value);

  // Sets the given |path| as the custom save location of captured images and
  // videos for the currently logged in user. Setting an empty |path| clears any
  // custom selected folder resulting in using the default downloads folder.
  // Calling this function will reset the value of "UsesDefaultCaptureFolder" to
  // false, since it means the user wants to switch to a custom folder when it's
  // set.
  // This can only be called when user is logged in.
  void SetCustomCaptureFolder(const base::FilePath& path);

  // Returns the currently saved custom folder even if it's not being currently
  // used.
  base::FilePath GetCustomCaptureFolder() const;

  // Returns the folder in which all taken screenshots and videos will be saved.
  // It can be the temp directory if the user is not logged in, the default
  // "Downloads" folder, or a user-selected custom location.
  CaptureFolder GetCurrentCaptureFolder() const;

  // Performs the instant full screen capture for each available display if no
  // restricted content exists on that display, each capture is saved as an
  // individual file. Note: this won't start a capture mode session.
  void CaptureScreenshotsOfAllDisplays();

  // Performs the instant screen capture for the `given_window` which bypasses
  // the capture mode session.
  void CaptureScreenshotOfGivenWindow(aura::Window* given_window);

  // Called only while a capture session is in progress to perform the actual
  // capture depending on the current selected `source_` and `type_`, and ends
  // the capture session.
  void PerformCapture();

  // Called by a capture session behavior to perform an image capture search.
  void PerformImageSearch();

  void EndVideoRecording(EndRecordingReason reason);

  // Posts a task to the blocking pool to check the availability of the given
  // `folder` and replies back asynchronously by calling the given `callback`
  // with `available` set either to true or false.
  void CheckFolderAvailability(
      const base::FilePath& folder,
      base::OnceCallback<void(bool available)> callback);

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

  // Returns true if the given `path` is the root folder of DriveFS, false
  // otherwise.
  bool IsRootDriveFsPath(const base::FilePath& path) const;

  // Returns true if the given `path` is the same as the Android Play files
  // path, false otherwise.
  bool IsAndroidFilesPath(const base::FilePath& path) const;

  // Returns true if the given `path` is the same as the Linux Files path, false
  // otherwise.
  bool IsLinuxFilesPath(const base::FilePath& path) const;

  // Returns true if the given `path` is the root folder of OneDrive, false
  // otherwise.
  bool IsRootOneDriveFilesPath(const base::FilePath& path) const;

  // Calls the delegate to instantiate `SearchResultsView`.
  std::unique_ptr<AshWebView> CreateSearchResultsView() const;

  // Returns the current parent window for the on-capture-surface widgets such
  // as `CaptureModeCameraController::camera_preview_widget_` and
  // `CaptureModeDemoToolsController::key_combo_widget_`.
  aura::Window* GetOnCaptureSurfaceWidgetParentWindow() const;

  // Returns the bounds, within which the on-capture-surface widgets (such as
  // the camera preview widget and the key combo widget) will be confined. The
  // bounds is in screen coordinate when capture source is `kFullscreen` or
  // 'kRegion', but in window's coordinate when it is 'kWindow' type.
  gfx::Rect GetCaptureSurfaceConfineBounds() const;

  // Returns the windows that to be avoided for collision with other system
  // windows such as the PIP window and the automatic click bubble menu.
  std::vector<aura::Window*> GetWindowsForCollisionAvoidance() const;

  // Updates the video conference manager and panel with the current state of
  // screen recording and whether camera or audio are being recorded.
  void MaybeUpdateVcPanel();

  // Checks if there are any content currently on the screen that are restricted
  // by DLP. `callback` will be triggered by the DLP manager with `proceed` set
  // to true if screen capture is allowed to continue, or set to false if it
  // should not continue.
  void CheckScreenCaptureDlpRestrictions(
      OnCaptureModeDlpRestrictionChecked callback);

  // Returns true if the video recording is in progress and annotating is
  // supported.
  bool ShouldAllowAnnotating() const;

  // Returns true is annotating should be supported for the current capture mode
  // behavior.
  bool IsAnnotatingSupported() const;

  // recording::mojom::RecordingServiceClient:
  void OnRecordingEnded(recording::mojom::RecordingStatus status,
                        const gfx::ImageSkia& thumbnail) override;

  // recording::mojom::DriveFsQuotaDelegate:
  void GetDriveFsFreeSpaceBytes(
      GetDriveFsFreeSpaceBytesCallback callback) override;

  // SessionObserver:
  void OnFirstSessionStarted() override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnChromeTerminating() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;

  // crosapi::mojom::VideoConferenceManagerClient:
  void GetMediaApps(GetMediaAppsCallback callback) override;
  void ReturnToApp(const base::UnguessableToken& token,
                   ReturnToAppCallback callback) override;
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override;
  void StopAllScreenShare() override;

  // Skips the 3-second count down, and IsCaptureAllowed() checks, and starts
  // video recording right away for testing purposes.
  void StartVideoRecordingImmediatelyForTesting();

  void AddObserver(CaptureModeObserver* observer);
  void RemoveObserver(CaptureModeObserver* observer);

  CaptureModeDelegate* delegate_for_testing() const { return delegate_.get(); }
  VideoRecordingWatcher* video_recording_watcher_for_testing() const {
    return video_recording_watcher_.get();
  }

 private:
  friend class CaptureModeTestApi;
  friend class VideoRecordingWatcher;

  // Performs the necessary setup common to all start methods (excluding
  // testing-only methods).
  void StartInternal(
      SessionType session_type,
      CaptureModeEntryType entry_type,
      OnSessionStartAttemptCallback callback = base::DoNothing());

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
  // recorded (i.e. |is_recording_in_progress()| is true) is about to move to a
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

  // The capture parameters for the capture operation that is about to be
  // performed (i.e. the window to be captured, and the capture bounds). If
  // nothing is to be captured (e.g. when there's no window selected in a
  // kWindow source, or no region is selected in a kRegion source), then a
  // std::nullopt is returned.
  struct CaptureParams {
    raw_ptr<aura::Window> window = nullptr;
    // The capture bounds, either in root coordinates (in kFullscreen or kRegion
    // capture sources), or window-local coordinates (in a kWindow capture
    // source).
    gfx::Rect bounds;
  };

  std::optional<CaptureParams> GetCaptureParams() const;

  // Launches the mojo service that handles audio and video recording, and
  // begins recording according to the given `capture_params`. It creates an
  // overlay on the video capturer so that it can be used to record the mouse
  // cursor. It gives the pending receiver end to that overlay on Viz, and the
  // other end should be owned by the `video_recording_watcher_`. If the given
  // `effective_audio_mode` (which takes into account the audio capture admin
  // policy and the recording format type) is not `kOff`, it will setup the
  // needed mojo connections to the Audio Service so that audio recording can be
  // done by the recording service.
  void LaunchRecordingServiceAndStartRecording(
      const CaptureParams& capture_params,
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay>
          cursor_overlay,
      AudioRecordingMode effective_audio_mode);

  // Called back when the mojo pipe to the recording service gets disconnected.
  void OnRecordingServiceDisconnected();

  // Terminates the recording service process, closes any recording-related UI
  // elements (only if |success| is false as this indicates that recording was
  // not ended normally by calling EndVideoRecording()), and shows the video
  // file notification with the given |thumbnail|.
  void FinalizeRecording(bool success, const gfx::ImageSkia& thumbnail);

  // Called to terminate the stop-recording shelf pod button, and the
  // |video_recording_watcher_| when recording ends.
  void TerminateRecordingUiElements();

  // The below functions start the actual image/video capture. They expect that
  // the capture session is still active when called, so they can retrieve the
  // capture parameters they need. They will end the sessions themselves.
  // They should never be called if IsCaptureAllowed() returns false.
  void CaptureImage(const CaptureParams& capture_params,
                    const base::FilePath& path,
                    const CaptureModeBehavior* behavior);
  void CaptureVideo(const CaptureParams& capture_params);

  // Called back when an image has been captured to trigger an attempt to save
  // the image as a file. `was_cursor_originally_blocked` is whether the cursor
  // was blocked at the time the screenshot capture request was made.
  // `png_bytes` is the buffer containing the captured image in a PNG format.
  void OnImageCaptured(const base::FilePath& path,
                       bool was_cursor_originally_blocked,
                       const CaptureModeBehavior* behavior,
                       scoped_refptr<base::RefCountedMemory> png_bytes);

  // Called back when an image has been captured to trigger a search request.
  // `jpeg_bytes` is the buffer containing the captured image in a JPEG format.
  void OnImageCapturedForSearch(
      bool was_cursor_originally_blocked,
      scoped_refptr<base::RefCountedMemory> jpeg_bytes);

  // Called back when the Scanner feature has processed a captured image to
  // suggest available Scanner actions.
  void OnScannerActionsFetched(std::vector<ScannerAction> scanner_actions);

  // Called back when an attempt to save the image file has been completed, with
  // `file_saved_path` indicating whether the attempt succeeded or failed. If
  // `file_saved_path` is empty, the attempt failed. `png_bytes` is the buffer
  // containing the captured image in a PNG format, which will be used to show a
  // preview of the image in a notification, and save it as a bitmap in the
  // clipboard. If saving was successful, then the image was saved in
  // `file_saved_path`.
  void OnImageFileSaved(scoped_refptr<base::RefCountedMemory> png_bytes,
                        const CaptureModeBehavior* behavior,
                        const base::FilePath& file_saved_path);

  // Called back after the image file was saved to the final location.
  // Parameters are same as for `OnImageFileSaved()` with `success` indicating
  // the result of finalizing the file.
  void OnImageFileFinalized(const gfx::Image& image,
                            const CaptureModeBehavior* behavior,
                            bool success,
                            const base::FilePath& file_saved_path);

  // Called back when the check for custom folder's availability is done in
  // `CheckFolderAvailability`, with `available` indicating whether the custom
  // folder is available or not.
  void OnCustomFolderAvailabilityChecked(bool available);

  // Called back when the `video_file_handler_` flushes the remaining cached
  // video chunks in its buffer. Called on the UI thread. `video_thumbnail` is
  // an RGB image provided by the recording service that can be used as a
  // thumbnail of the video in the notification. `behavior` will decide whether
  // the recording will not be shown in tote or notification.
  // `saved_video_file_path` indicates whether the attempt succeeded or failed.
  // If `saved_video_file_path` is empty, the attempt failed.
  void OnVideoFileSaved(const gfx::ImageSkia& video_thumbnail,
                        const CaptureModeBehavior* behavior,
                        bool success,
                        const base::FilePath& saved_video_file_path);

  // Shows a preview notification of the newly taken screenshot or screen
  // recording. Customized notification view will be decided based on the
  // `behavior`.
  void ShowPreviewNotification(const base::FilePath& screen_capture_path,
                               const gfx::Image& preview_image,
                               const CaptureModeType type,
                               const CaptureModeBehavior* behavior);
  void HandleNotificationClicked(const base::FilePath& screen_capture_path,
                                 const CaptureModeType type,
                                 const BehaviorType behavior_type,
                                 std::optional<int> button_index);

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

  // Returns a fallback path concatenating the default `Downloads` folder and
  // the base name of `path`.
  base::FilePath GetFallbackFilePathFromFile(const base::FilePath& path);

  // Records the number of screenshots taken.
  void RecordAndResetScreenshotsTakenInLastDay();
  void RecordAndResetScreenshotsTakenInLastWeek();

  // Records the number of consecutive screenshots taken within 5s of each
  // other.
  void RecordAndResetConsecutiveScreenshots();

  // Called when the video record 3-seconds count down finishes.
  void OnVideoRecordCountDownFinished();

  // Called when the client of capture mode requires creating its own folder to
  // host the video file, and that folder has been created, and the desired full
  // path of the video file (including the extension) is provided. Note that
  // this path will be empty if the folder creation operation failed.
  void OnCaptureFolderCreated(const CaptureParams& capture_params,
                              const base::FilePath& capture_file_full_path);

  // Ends the capture session and starts the video recording for the given
  // `capture_params`. The video will be saved to a file to the given
  // `video_file_path`. This can only be called while the session is still
  // active.
  void BeginVideoRecording(const CaptureParams& capture_params,
                           const base::FilePath& video_file_path);

  // Called to interrupt the ongoing video recording because it's not anymore
  // allowed to be captured.
  void InterruptVideoRecording();

  // Called by the DLP manager when it's checked for any on-screen content
  // restriction at the time when the capture operation is attempted. `proceed`
  // will be set to true if the capture operation should continue, false if it
  // should be aborted.
  void OnDlpRestrictionCheckedAtPerformingCapture(bool proceed);

  // Called after the video recording was written to the final file location.
  // `should_delete_file` indicates if the file was deleted due to an error.
  void OnVideoFileFinalized(bool should_delete_file,
                            const gfx::ImageSkia& video_thumbnail);

  // Called by the DLP manager when it's checked again for any on-screen content
  // restriction at the time when the video capture 3-second countdown ends.
  // `proceed` will be set to true if video recording should begin, or false if
  // it should be aborted.
  void OnDlpRestrictionCheckedAtCountDownFinished(bool proceed);

  // Bound to a callback that will be called by the DLP manager to let us know
  // whether a pending session initialization should `proceed` or abort due to
  // some restricted contents on the screen. `at_exit_closure` is passed from
  // `Start()` and will be called on the exit of the function.
  void OnDlpRestrictionCheckedAtSessionInit(SessionType session_type,
                                            CaptureModeEntryType entry_type,
                                            base::OnceClosure at_exit_closure,
                                            bool proceed);

  // At the end of a video recording, the DLP manager is checked to see if there
  // were any restricted content of a warning level type during the recording
  // (warning-level restrictions do not result in interrupting the video
  // recording), if so, the DLP manager shows a dialog asking the user whether
  // to continue with saving the video file. When the dialog closes, this
  // function is called to provide the user choice; save the video file when
  // `proceed` is true, or to delete it when `proceed` is false.
  void OnDlpRestrictionCheckedAtVideoEnd(const gfx::ImageSkia& video_thumbnail,
                                         bool success,
                                         const CaptureModeBehavior* behavior,
                                         bool proceed);

  // Encapsulates the policy check and calls into DLP manager to do DLP check.
  // `instant_screenshot_callback` will be moved and invoked in
  // `OnDlpRestrictionCheckedAtCaptureScreenshot()` to perform the instant
  // screenshot. This is invoked via the screenshot accelerator commands and
  // will end capture mode session if it is active (only if the session was
  // started by the same behavior).
  void CaptureInstantScreenshot(CaptureModeEntryType entry_type,
                                CaptureModeSource source,
                                base::OnceClosure instant_screenshot_callback,
                                BehaviorType behavior_type);
  // Bound to a callback that will be called by DLP manager to let the user know
  // whether screen capture should `proceed` or abort due to some restricted
  // contents on the screen. Invokes the `instant_screenshot_callback` and
  // records the metrics for the given `behavior_type` if `proceed` is true and
  // screen capture is allowed by the enterprise policy.
  void OnDlpRestrictionCheckedAtCaptureScreenshot(
      CaptureModeEntryType entry_type,
      CaptureModeSource source,
      base::OnceClosure instant_screenshot_callback,
      BehaviorType behavior_type,
      bool proceed);

  // Takes screenshots of all the available displays and saves them to disk.
  void PerformScreenshotsOfAllDisplays(BehaviorType behavior_type);

  // Takes a screenshot of the `given_window` and save it to disk.
  void PerformScreenshotOfGivenWindow(aura::Window* given_window,
                                      BehaviorType behavior_type);

  bool ShouldClearCaptureRegion(BehaviorType behavior_type) const;

  // Gets the corresponding `SaveLocation` enum value on the given `path`.
  CaptureModeSaveToLocation GetSaveToOption(const base::FilePath& path);

  // Retrieves the instance of the `CaptureModeBehavior` by the given
  // `behavior_type` from the `behaviors_map_` if exits, or creates an instance
  // otherwise.
  CaptureModeBehavior* GetBehavior(BehaviorType behavior_type);

  // The ID of this object as a client of the video conference manager.
  const base::UnguessableToken vc_client_id_ = base::UnguessableToken::Create();

  // The ID of an ongoing recording as a media app tracked by the video
  // conference manager.
  const base::UnguessableToken capture_mode_media_app_id_ =
      base::UnguessableToken::Create();

  std::unique_ptr<CaptureModeDelegate> delegate_;

  // Controls the selfie camera feature of capture mode.
  std::unique_ptr<CaptureModeCameraController> camera_controller_;

  CaptureModeType type_ = CaptureModeType::kImage;
  CaptureModeSource source_ = CaptureModeSource::kRegion;
  RecordingType recording_type_ = RecordingType::kWebM;

  // A blocking task runner for file IO operations.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  mojo::Remote<recording::mojom::RecordingService> recording_service_remote_;
  mojo::Receiver<recording::mojom::RecordingServiceClient>
      recording_service_client_receiver_{this};
  mojo::Receiver<recording::mojom::DriveFsQuotaDelegate>
      drive_fs_quota_delegate_receiver_{this};

  // This is the file path of the video file currently being recorded. It is
  // empty when no video recording is in progress or when no video is being
  // saved.
  base::FilePath current_video_file_path_;

  // We remember the user selected capture region when the source is |kRegion|
  // between sessions. Initially, this value is empty at which point we display
  // a message to the user instructing them to start selecting a region.
  gfx::Rect user_capture_region_;

  std::unique_ptr<BaseCaptureModeSession> capture_mode_session_;

  // Remember the user selected preference for audio recording between sessions.
  // Initially, this value is set to `kOff`, ensuring that this is an opt-in
  // feature. Note that this value will always be overwritten by the
  // `AudioCaptureAllowed` policy, when `GetEffectiveAudioRecordingMode()` is
  // called.
  AudioRecordingMode audio_recording_mode_ = AudioRecordingMode::kOff;

  // If true, the 3-second countdown UI will be skipped, and video recording
  // will start immediately.
  bool skip_count_down_ui_ = false;

  // True only if the recording service detects a |kLowDiskSpace| condition
  // while writing the video file to the file system. This value is used only to
  // determine the message shown to the user in the video preview notification
  // to explain why the recording was ended, and is then reset back to false.
  bool low_disk_space_threshold_reached_ = false;

  // Set to true when we're waiting for a callback from the DLP manager to check
  // content restrictions that may block capture mode at any of its stages
  // (initialization or performing the capture).
  bool pending_dlp_check_ = false;

  // These track the state of the camera and microphone (e.g. the camera can be
  // disabled using a privacy switch, and the microphone can be muted from the
  // settings).
  bool is_camera_muted_ = false;
  bool is_microphone_muted_ = false;

  // Watches events that lead to ending video recording.
  std::unique_ptr<VideoRecordingWatcher> video_recording_watcher_;

  // Tracks the windows that currently have content protection enabled, so that
  // we prevent them from being video recorded. Each window is mapped to its
  // currently-set protection_mask. Windows in this map are only the ones that
  // have protection masks other than |display::CONTENT_PROTECTION_METHOD_NONE|.
  base::flat_map<aura::Window*, /*protection_mask*/ uint32_t>
      protected_windows_;

  // If set, it will be called when either an image or video file is saved and
  // the file size metric has been recorded.
  base::OnceCallback<void(const base::FilePath&)>
      on_file_saved_callback_for_test_;

  OnFileDeletedCallback on_file_deleted_callback_for_test_;

  base::OnceClosure on_countdown_finished_callback_for_test_;

  base::OnceClosure on_video_recording_started_callback_for_test_;

  base::OnceClosure on_image_captured_for_search_callback_for_test_;

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

  // True in the scope of BeginVideoRecording().
  bool is_initializing_recording_ = false;

  // Remember the user preference of whether to enable demo tools feature or
  // not in video recording mode, between sessions. Initially, this value is set
  // to false, ensuring that this is an opt-in feature.
  bool enable_demo_tools_ = false;

  // Maps an instance of the `CaptureModeBehavior` by `BehaviorType`. This is a
  // map because a user session may start multiple different behaviors, during
  // which we don't need to instantiate the behavior again. We also want to keep
  // the behavior alive for the rest of the session until the user signs out or
  // shuts down. This is because the behaviors are the ones that cache the
  // settings.
  base::flat_map<BehaviorType, std::unique_ptr<CaptureModeBehavior>>
      behaviors_map_;

  base::ObserverList<CaptureModeObserver> observers_;

  std::unique_ptr<CaptureModeEducationController> education_controller_;

  base::WeakPtrFactory<CaptureModeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CONTROLLER_H_
