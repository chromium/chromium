// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/test_capture_mode_delegate.h"

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/services/recording/public/mojom/recording_service.mojom.h"
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

  const viz::FrameSinkId& current_frame_sink_id() const {
    return current_frame_sink_id_;
  }
  gfx::Size frame_sink_size() const { return frame_sink_size_; }
  const gfx::Size& video_size() const { return video_size_; }
  void set_thumbnail(const gfx::ImageSkia& thumbnail) {
    thumbnail_ = thumbnail;
  }

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
      const gfx::Size& frame_sink_size) override {
    remote_client_.Bind(std::move(client));
    current_frame_sink_id_ = frame_sink_id;
    current_capture_source_ = CaptureModeSource::kFullscreen;
    frame_sink_size_ = frame_sink_size;
    video_size_ = frame_sink_size;
  }
  void RecordWindow(
      mojo::PendingRemote<recording::mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& frame_sink_size,
      const viz::SubtreeCaptureId& subtree_capture_id,
      const gfx::Size& window_size) override {
    remote_client_.Bind(std::move(client));
    current_frame_sink_id_ = frame_sink_id;
    current_capture_source_ = CaptureModeSource::kWindow;
    frame_sink_size_ = frame_sink_size;
    video_size_ = window_size;
  }
  void RecordRegion(
      mojo::PendingRemote<recording::mojom::RecordingServiceClient> client,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer,
      mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory,
      const viz::FrameSinkId& frame_sink_id,
      const gfx::Size& frame_sink_size,
      const gfx::Rect& crop_region) override {
    remote_client_.Bind(std::move(client));
    current_frame_sink_id_ = frame_sink_id;
    current_capture_source_ = CaptureModeSource::kRegion;
    frame_sink_size_ = frame_sink_size;
    video_size_ = crop_region.size();
  }
  void StopRecording() override {
    remote_client_->OnRecordingEnded(/*success=*/true, thumbnail_);
    remote_client_.FlushForTesting();
  }
  void OnRecordedWindowChangingRoot(
      const viz::FrameSinkId& new_frame_sink_id,
      const gfx::Size& new_frame_sink_size) override {
    DCHECK_EQ(current_capture_source_, CaptureModeSource::kWindow);
    current_frame_sink_id_ = new_frame_sink_id;
    frame_sink_size_ = new_frame_sink_size;
  }
  void OnRecordedWindowSizeChanged(const gfx::Size& new_window_size) override {
    DCHECK_EQ(current_capture_source_, CaptureModeSource::kWindow);
    video_size_ = new_window_size;
  }
  void OnFrameSinkSizeChanged(const gfx::Size& new_frame_sink_size) override {
    DCHECK_NE(current_capture_source_, CaptureModeSource::kFullscreen);
    frame_sink_size_ = new_frame_sink_size;
  }

 private:
  mojo::Receiver<recording::mojom::RecordingService> receiver_;
  mojo::Remote<recording::mojom::RecordingServiceClient> remote_client_;
  viz::FrameSinkId current_frame_sink_id_;
  CaptureModeSource current_capture_source_ = CaptureModeSource::kFullscreen;
  gfx::Size frame_sink_size_;
  gfx::Size video_size_;
  gfx::ImageSkia thumbnail_;
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

viz::FrameSinkId TestCaptureModeDelegate::GetCurrentFrameSinkId() const {
  return fake_service_ ? fake_service_->current_frame_sink_id()
                       : viz::FrameSinkId();
}

gfx::Size TestCaptureModeDelegate::GetCurrentFrameSinkSize() const {
  return fake_service_ ? fake_service_->frame_sink_size() : gfx::Size();
}

gfx::Size TestCaptureModeDelegate::GetCurrentVideoSize() const {
  return fake_service_ ? fake_service_->video_size() : gfx::Size();
}

void TestCaptureModeDelegate::SetVideoThumbnail(
    const gfx::ImageSkia& thumbnail) {
  if (fake_service_)
    fake_service_->set_thumbnail(thumbnail);
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
  fake_service_ = std::make_unique<FakeRecordingService>();
  mojo::Remote<recording::mojom::RecordingService> service;
  fake_service_->Bind(service.BindNewPipeAndPassReceiver());
  return service;
}

void TestCaptureModeDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {}

void TestCaptureModeDelegate::OnSessionStateChanged(bool started) {}

}  // namespace ash
