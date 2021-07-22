// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/test_capture_mode_delegate.h"

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/services/recording/public/mojom/recording_service.mojom.h"
#include "ash/services/recording/recording_service_test_api.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"

namespace ash {

TestCaptureModeDelegate::TestCaptureModeDelegate() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool result =
      base::CreateNewTempDirectory(/*prefix=*/"", &fake_downloads_dir_);
  DCHECK(result);
}

TestCaptureModeDelegate::~TestCaptureModeDelegate() = default;

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

base::FilePath TestCaptureModeDelegate::GetScreenCaptureDir() const {
  return fake_downloads_dir_;
}

void TestCaptureModeDelegate::ShowScreenCaptureItemInFolder(
    const base::FilePath& file_path) {}

void TestCaptureModeDelegate::OpenScreenshotInImageEditor(
    const base::FilePath& file_path) {}

bool TestCaptureModeDelegate::Uses24HourFormat() const {
  return false;
}

bool TestCaptureModeDelegate::IsCaptureModeInitRestrictedByDlp() const {
  return false;
}

bool TestCaptureModeDelegate::IsCaptureAllowedByDlp(const aura::Window* window,
                                                    const gfx::Rect& bounds,
                                                    bool for_video) const {
  return true;
}

bool TestCaptureModeDelegate::IsCaptureAllowedByPolicy() const {
  return true;
}

void TestCaptureModeDelegate::StartObservingRestrictedContent(
    const aura::Window* window,
    const gfx::Rect& bounds,
    base::OnceClosure stop_callback) {}

void TestCaptureModeDelegate::StopObservingRestrictedContent() {}

mojo::Remote<recording::mojom::RecordingService>
TestCaptureModeDelegate::LaunchRecordingService() {
  mojo::Remote<recording::mojom::RecordingService> service_remote;
  recording_service_ = std::make_unique<recording::RecordingServiceTestApi>(
      service_remote.BindNewPipeAndPassReceiver());
  return service_remote;
}

void TestCaptureModeDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {}

void TestCaptureModeDelegate::OnSessionStateChanged(bool started) {}

void TestCaptureModeDelegate::OnServiceRemoteReset() {
  // We simulate what the ServiceProcessHost does when the service remote is
  // reset (on which it shuts down the service process). Here since the service
  // is running in-process with ash_unittests, we just delete the instance.
  recording_service_.reset();
}

}  // namespace ash
