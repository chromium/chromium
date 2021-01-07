// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/test_capture_mode_delegate.h"

#include "ash/services/recording/public/mojom/recording_service.mojom.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"

namespace ash {

// -----------------------------------------------------------------------------
// FakeRecordingService:

class FakeRecordingService : public recording::mojom::RecordingService {
 public:
  FakeRecordingService() : receiver_(this) {}

  FakeRecordingService(const FakeRecordingService&) = delete;
  FakeRecordingService& operator=(const FakeRecordingService&) = delete;
  ~FakeRecordingService() override = default;

  void Bind(
      mojo::PendingReceiver<recording::mojom::RecordingService> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // mojom::RecordingService:
  void RecordFullscreen(
      mojo::PendingRemote<recording::mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& fullscreen_size) override {
    remote_client_.Bind(std::move(client));
  }
  void RecordWindow(
      mojo::PendingRemote<recording::mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
      const viz::FrameSinkId& frame_sink_id,
      const viz::SubtreeCaptureId& subtree_capture_id,
      const gfx::Size& initial_window_size,
      const gfx::Size& max_window_size) override {
    remote_client_.Bind(std::move(client));
  }
  void RecordRegion(
      mojo::PendingRemote<recording::mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& fullscreen_size,
      const gfx::Rect& corp_region) override {
    remote_client_.Bind(std::move(client));
  }
  void StopRecording() override {
    remote_client_->OnRecordingEnded(/*success=*/true);
    remote_client_.FlushForTesting();
  }

 private:
  mojo::Receiver<recording::mojom::RecordingService> receiver_;
  mojo::Remote<recording::mojom::RecordingServiceClient> remote_client_;
};

// -----------------------------------------------------------------------------
// TestCaptureModeDelegate:

TestCaptureModeDelegate::TestCaptureModeDelegate() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool result =
      base::CreateNewTempDirectory(/*prefix=*/"", &fake_downloads_dir_);
  DCHECK(result);
}

TestCaptureModeDelegate::~TestCaptureModeDelegate() = default;

base::FilePath TestCaptureModeDelegate::GetActiveUserDownloadsDir() const {
  return fake_downloads_dir_;
}

void TestCaptureModeDelegate::ShowScreenCaptureItemInFolder(
    const base::FilePath& file_path) {}

void TestCaptureModeDelegate::OpenScreenshotInImageEditor(
    const base::FilePath& file_path) {}

bool TestCaptureModeDelegate::Uses24HourFormat() const {
  return false;
}

bool TestCaptureModeDelegate::IsCaptureModeInitRestricted() const {
  return false;
}

bool TestCaptureModeDelegate::IsCaptureAllowed(const aura::Window* window,
                                               const gfx::Rect& bounds,
                                               bool for_video) const {
  return true;
}

void TestCaptureModeDelegate::StartObservingRestrictedContent(
    const aura::Window* window,
    const gfx::Rect& bounds,
    base::OnceClosure stop_callback) {}

void TestCaptureModeDelegate::StopObservingRestrictedContent() {}

void TestCaptureModeDelegate::OpenFeedbackDialog() {}

mojo::Remote<recording::mojom::RecordingService>
TestCaptureModeDelegate::LaunchRecordingService() {
  fake_service_ = std::make_unique<FakeRecordingService>();
  mojo::Remote<recording::mojom::RecordingService> service;
  fake_service_->Bind(service.BindNewPipeAndPassReceiver());
  return service;
}

void TestCaptureModeDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {}

}  // namespace ash
