// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
#define ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_

#include <limits>
#include <memory>

#include "ash/public/cpp/capture_mode/capture_mode_delegate.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace recording {
class RecordingServiceTestApi;
}  // namespace recording

namespace ash {

class FakeVideoSourceProvider;

class TestCaptureModeDelegate : public CaptureModeDelegate {
 public:
  TestCaptureModeDelegate();
  TestCaptureModeDelegate(const TestCaptureModeDelegate&) = delete;
  TestCaptureModeDelegate& operator=(const TestCaptureModeDelegate&) = delete;
  ~TestCaptureModeDelegate() override;

  bool is_session_active() const { return is_session_active_; }

  recording::RecordingServiceTestApi* recording_service() const {
    return recording_service_.get();
  }
  FakeVideoSourceProvider* video_source_provider() {
    return video_source_provider_.get();
  }
  void set_on_session_state_changed_callback(base::OnceClosure callback) {
    on_session_state_changed_callback_ = std::move(callback);
  }
  void set_is_allowed_by_dlp(bool value) { is_allowed_by_dlp_ = value; }
  void set_is_allowed_by_policy(bool value) { is_allowed_by_policy_ = value; }
  void set_should_save_after_dlp_check(bool value) {
    should_save_after_dlp_check_ = value;
  }
  void set_is_camera_disabled_by_policy(bool value) {
    is_camera_disabled_by_policy_ = value;
  }
  void set_is_audio_capture_disabled_by_policy(bool value) {
    is_audio_capture_disabled_by_policy_ = value;
  }
  void set_fake_drive_fs_free_bytes(int64_t bytes) {
    fake_drive_fs_free_bytes_ = bytes;
  }
  void set_policy_capture_path(PolicyCapturePath policy_capture_path) {
    policy_capture_path_ = policy_capture_path;
  }
  int num_capture_image_attempts() const { return num_capture_image_attempts_; }

  // Resets |is_allowed_by_policy_| and |is_allowed_by_dlp_| back to true.
  void ResetAllowancesToDefault();

  // Gets the current frame sink id being captured by the service.
  viz::FrameSinkId GetCurrentFrameSinkId() const;

  // Gets the current size of the frame sink being recorded in pixels.
  gfx::Size GetCurrentFrameSinkSizeInPixels() const;

  // Gets the current video size being captured by the service.
  gfx::Size GetCurrentVideoSize() const;

  // Gets the thumbnail image that will be used by the service to provide it to
  // the client.
  gfx::ImageSkia GetVideoThumbnail() const;

  // Requests a video frame from the video capturer and waits for it to be
  // delivered to the service.
  void RequestAndWaitForVideoFrame();

  // Returns true if there is an ongoing recording and the recording service is
  // currently recording audio.
  bool IsDoingAudioRecording() const;

  // Returns the number of audio capturers owned by the recording service.
  int GetNumberOfAudioCapturers() const;

  // CaptureModeDelegate:
  base::FilePath GetUserDefaultDownloadsFolder() const override;
  void OpenScreenCaptureItem(const base::FilePath& file_path) override;
  void OpenScreenshotInImageEditor(const base::FilePath& file_path) override;
  bool Uses24HourFormat() const override;
  void CheckCaptureModeInitRestrictionByDlp(
      OnCaptureModeDlpRestrictionChecked callback) override;
  void CheckCaptureOperationRestrictionByDlp(
      const aura::Window* window,
      const gfx::Rect& bounds,
      OnCaptureModeDlpRestrictionChecked callback) override;
  bool IsCaptureAllowedByPolicy() const override;
  void StartObservingRestrictedContent(
      const aura::Window* window,
      const gfx::Rect& bounds,
      base::OnceClosure stop_callback) override;
  void StopObservingRestrictedContent(
      OnCaptureModeDlpRestrictionChecked callback) override;
  void OnCaptureImageAttempted(aura::Window const*, gfx::Rect const&) override;
  mojo::Remote<recording::mojom::RecordingService> LaunchRecordingService()
      override;
  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override;
  void OnSessionStateChanged(bool started) override;
  void OnServiceRemoteReset() override;
  bool GetDriveFsMountPointPath(base::FilePath* result) const override;
  base::FilePath GetAndroidFilesPath() const override;
  base::FilePath GetLinuxFilesPath() const override;
  base::FilePath GetOneDriveMountPointPath() const override;
  PolicyCapturePath GetPolicyCapturePath() const override;
  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override;
  void GetDriveFsFreeSpaceBytes(OnGotDriveFsFreeSpace callback) override;
  bool IsCameraDisabledByPolicy() const override;
  bool IsAudioCaptureDisabledByPolicy() const override;
  void RegisterVideoConferenceManagerClient(
      crosapi::mojom::VideoConferenceManagerClient* client,
      const base::UnguessableToken& client_id) override;
  void UnregisterVideoConferenceManagerClient(
      const base::UnguessableToken& client_id) override;
  void UpdateVideoConferenceManager(
      crosapi::mojom::VideoConferenceMediaUsageStatusPtr status) override;
  void NotifyDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device) override;
  void FinalizeSavedFile(
      base::OnceCallback<void(bool, const base::FilePath&)> callback,
      const base::FilePath& path,
      const gfx::Image& thumbnail) override;
  base::FilePath RedirectFilePath(const base::FilePath& path) override;
  std::unique_ptr<AshWebView> CreateSearchResultsView() const override;

 private:
  std::unique_ptr<recording::RecordingServiceTestApi> recording_service_;
  std::unique_ptr<FakeVideoSourceProvider> video_source_provider_;
  base::ScopedTempDir fake_downloads_dir_;
  base::OnceClosure on_session_state_changed_callback_;
  bool is_session_active_ = false;
  bool is_allowed_by_dlp_ = true;
  bool is_allowed_by_policy_ = true;
  bool should_save_after_dlp_check_ = true;
  bool is_camera_disabled_by_policy_ = false;
  bool is_audio_capture_disabled_by_policy_ = false;
  // Counter to track number of times `OnCaptureImageAttempted()` is called, for
  // testing purposes.
  int num_capture_image_attempts_ = 0;
  base::ScopedTempDir fake_drive_fs_mount_path_;
  base::ScopedTempDir fake_android_files_path_;
  base::ScopedTempDir fake_linux_files_path_;
  base::ScopedTempDir fake_one_drive_mount_path_;
  int64_t fake_drive_fs_free_bytes_ = std::numeric_limits<int64_t>::max();
  PolicyCapturePath policy_capture_path_ = {base::FilePath(),
                                            CapturePathEnforcement::kNone};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
