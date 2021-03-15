// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
#define ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/capture_mode_delegate.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class FakeRecordingService;

class TestCaptureModeDelegate : public CaptureModeDelegate {
 public:
  TestCaptureModeDelegate();
  TestCaptureModeDelegate(const TestCaptureModeDelegate&) = delete;
  TestCaptureModeDelegate& operator=(const TestCaptureModeDelegate&) = delete;
  ~TestCaptureModeDelegate() override;

  // Gets the current frame sink id being captured by the fake service.
  viz::FrameSinkId GetCurrentFrameSinkId() const;

  // Gets the current size of the frame sink being recorded.
  gfx::Size GetCurrentFrameSinkSize() const;

  // Gets the current video size being captured by the fake service.
  gfx::Size GetCurrentVideoSize() const;

  // Sets the thumbnail image that will be used by the fake service to provide
  // it to the client.
  void SetVideoThumbnail(const gfx::ImageSkia& thumbnail);

  // CaptureModeDelegate:
  base::FilePath GetScreenCaptureDir() const override;
  void ShowScreenCaptureItemInFolder(const base::FilePath& file_path) override;
  void OpenScreenshotInImageEditor(const base::FilePath& file_path) override;
  bool Uses24HourFormat() const override;
  bool IsCaptureModeInitRestrictedByDlp() const override;
  bool IsCaptureAllowedByDlp(const aura::Window* window,
                             const gfx::Rect& bounds,
                             bool for_video) const override;
  bool IsCaptureAllowedByPolicy() const override;
  void StartObservingRestrictedContent(
      const aura::Window* window,
      const gfx::Rect& bounds,
      base::OnceClosure stop_callback) override;
  void StopObservingRestrictedContent() override;
  mojo::Remote<recording::mojom::RecordingService> LaunchRecordingService()
      override;
  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override;
  void OnSessionStateChanged(bool started) override;

 private:
  std::unique_ptr<FakeRecordingService> fake_service_;
  base::FilePath fake_downloads_dir_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
