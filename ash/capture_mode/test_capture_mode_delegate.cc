// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/test_capture_mode_delegate.h"

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/fake_video_source_provider.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_service_test_api.h"

namespace ash {

TestCaptureModeDelegate::TestCaptureModeDelegate()
    : video_source_provider_(std::make_unique<FakeVideoSourceProvider>()) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  bool created_dir = fake_downloads_dir_.CreateUniqueTempDir();
  DCHECK(created_dir);
  created_dir = fake_drive_fs_mount_path_.CreateUniqueTempDir();
  DCHECK(created_dir);
  created_dir = fake_android_files_path_.CreateUniqueTempDir();
  DCHECK(created_dir);
  created_dir = fake_linux_files_path_.CreateUniqueTempDir();
  DCHECK(created_dir);
  created_dir = fake_one_drive_mount_path_.CreateUniqueTempDir();
  DCHECK(created_dir);
}

TestCaptureModeDelegate::~TestCaptureModeDelegate() = default;

void TestCaptureModeDelegate::ResetAllowancesToDefault() {
  is_allowed_by_dlp_ = true;
  is_allowed_by_policy_ = true;
}

viz::FrameSinkId TestCaptureModeDelegate::GetCurrentFrameSinkId() const {
  return recording_service_ ? recording_service_->GetCurrentFrameSinkId()
                            : viz::FrameSinkId();
}

gfx::Size TestCaptureModeDelegate::GetCurrentFrameSinkSizeInPixels() const {
  return recording_service_
             ? recording_service_->GetCurrentFrameSinkSizeInPixels()
             : gfx::Size();
}

gfx::Size TestCaptureModeDelegate::GetCurrentVideoSize() const {
  return recording_service_ ? recording_service_->GetCurrentVideoSize()
                            : gfx::Size();
}

gfx::ImageSkia TestCaptureModeDelegate::GetVideoThumbnail() const {
  return recording_service_ ? recording_service_->GetVideoThumbnail()
                            : gfx::ImageSkia();
}

void TestCaptureModeDelegate::RequestAndWaitForVideoFrame() {
  DCHECK(recording_service_);

  recording_service_->RequestAndWaitForVideoFrame();
}

bool TestCaptureModeDelegate::IsDoingAudioRecording() const {
  return recording_service_ && recording_service_->IsDoingAudioRecording();
}

int TestCaptureModeDelegate::GetNumberOfAudioCapturers() const {
  return recording_service_ ? recording_service_->GetNumberOfAudioCapturers()
                            : 0;
}

base::FilePath TestCaptureModeDelegate::GetUserDefaultDownloadsFolder() const {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());

  return fake_downloads_dir_.GetPath();
}

void TestCaptureModeDelegate::OpenScreenCaptureItem(
    const base::FilePath& file_path) {}

void TestCaptureModeDelegate::OpenScreenshotInImageEditor(
    const base::FilePath& file_path) {}

bool TestCaptureModeDelegate::Uses24HourFormat() const {
  return false;
}

void TestCaptureModeDelegate::CheckCaptureModeInitRestrictionByDlp(
    OnCaptureModeDlpRestrictionChecked callback) {
  std::move(callback).Run(/*proceed=*/is_allowed_by_dlp_);
}

void TestCaptureModeDelegate::CheckCaptureOperationRestrictionByDlp(
    const aura::Window* window,
    const gfx::Rect& bounds,
    OnCaptureModeDlpRestrictionChecked callback) {
  std::move(callback).Run(/*proceed=*/is_allowed_by_dlp_);
}

bool TestCaptureModeDelegate::IsCaptureAllowedByPolicy() const {
  return is_allowed_by_policy_;
}

void TestCaptureModeDelegate::StartObservingRestrictedContent(
    const aura::Window* window,
    const gfx::Rect& bounds,
    base::OnceClosure stop_callback) {}

void TestCaptureModeDelegate::StopObservingRestrictedContent(
    OnCaptureModeDlpRestrictionChecked callback) {
  DCHECK(callback);
  std::move(callback).Run(should_save_after_dlp_check_);
}

void TestCaptureModeDelegate::OnCaptureImageAttempted(aura::Window const*,
                                                      gfx::Rect const&) {
  ++num_capture_image_attempts_;
}

mojo::Remote<recording::mojom::RecordingService>
TestCaptureModeDelegate::LaunchRecordingService() {
  mojo::Remote<recording::mojom::RecordingService> service_remote;
  recording_service_ = std::make_unique<recording::RecordingServiceTestApi>(
      service_remote.BindNewPipeAndPassReceiver());
  return service_remote;
}

void TestCaptureModeDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {}

void TestCaptureModeDelegate::OnSessionStateChanged(bool started) {
  is_session_active_ = started;

  if (on_session_state_changed_callback_)
    std::move(on_session_state_changed_callback_).Run();
}

void TestCaptureModeDelegate::OnServiceRemoteReset() {
  // We simulate what the ServiceProcessHost does when the service remote is
  // reset (on which it shuts down the service process). Here since the service
  // is running in-process with ash_unittests, we just delete the instance.
  recording_service_.reset();
}

bool TestCaptureModeDelegate::GetDriveFsMountPointPath(
    base::FilePath* result) const {
  *result = fake_drive_fs_mount_path_.GetPath();
  return true;
}

base::FilePath TestCaptureModeDelegate::GetAndroidFilesPath() const {
  return fake_android_files_path_.GetPath();
}

base::FilePath TestCaptureModeDelegate::GetLinuxFilesPath() const {
  return fake_linux_files_path_.GetPath();
}

base::FilePath TestCaptureModeDelegate::GetOneDriveMountPointPath() const {
  return fake_one_drive_mount_path_.GetPath();
}

TestCaptureModeDelegate::PolicyCapturePath
TestCaptureModeDelegate::GetPolicyCapturePath() const {
  return policy_capture_path_;
}

void TestCaptureModeDelegate::ConnectToVideoSourceProvider(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver) {
  video_source_provider_->Bind(std::move(receiver));
}

void TestCaptureModeDelegate::GetDriveFsFreeSpaceBytes(
    OnGotDriveFsFreeSpace callback) {
  std::move(callback).Run(fake_drive_fs_free_bytes_);
}

bool TestCaptureModeDelegate::IsCameraDisabledByPolicy() const {
  return is_camera_disabled_by_policy_;
}

bool TestCaptureModeDelegate::IsAudioCaptureDisabledByPolicy() const {
  return is_audio_capture_disabled_by_policy_;
}

void TestCaptureModeDelegate::RegisterVideoConferenceManagerClient(
    crosapi::mojom::VideoConferenceManagerClient* client,
    const base::UnguessableToken& client_id) {}

void TestCaptureModeDelegate::UnregisterVideoConferenceManagerClient(
    const base::UnguessableToken& client_id) {}

void TestCaptureModeDelegate::UpdateVideoConferenceManager(
    crosapi::mojom::VideoConferenceMediaUsageStatusPtr status) {}

void TestCaptureModeDelegate::NotifyDeviceUsedWhileDisabled(
    crosapi::mojom::VideoConferenceMediaDevice device) {}

void TestCaptureModeDelegate::FinalizeSavedFile(
    base::OnceCallback<void(bool, const base::FilePath&)> callback,
    const base::FilePath& path,
    const gfx::Image& thumbnail) {
  std::move(callback).Run(/*success=*/true, path);
}

base::FilePath TestCaptureModeDelegate::RedirectFilePath(
    const base::FilePath& path) {
  return path;
}

std::unique_ptr<AshWebView> TestCaptureModeDelegate::CreateSearchResultsView()
    const {
  // In ash unit and pixel tests we only need an `AshWebView` instance.
  return AshWebViewFactory::Get()->Create(AshWebView::InitParams());
}

}  // namespace ash
