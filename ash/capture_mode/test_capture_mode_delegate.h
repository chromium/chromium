// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
#define ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/capture_mode_delegate.h"
#include "base/callback.h"
#include "base/files/file_path.h"

namespace ash {

class FakeRecordingService;

class TestCaptureModeDelegate : public CaptureModeDelegate {
 public:
  TestCaptureModeDelegate();
  TestCaptureModeDelegate(const TestCaptureModeDelegate&) = delete;
  TestCaptureModeDelegate& operator=(const TestCaptureModeDelegate&) = delete;
  ~TestCaptureModeDelegate() override;

  // CaptureModeDelegate:
  base::FilePath GetActiveUserDownloadsDir() const override;
  void ShowScreenCaptureItemInFolder(const base::FilePath& file_path) override;
  void OpenScreenshotInImageEditor(const base::FilePath& file_path) override;
  bool Uses24HourFormat() const override;
  bool IsCaptureModeInitRestricted() const override;
  bool IsCaptureAllowed(const aura::Window* window,
                        const gfx::Rect& bounds,
                        bool for_video) const override;
  void StartObservingRestrictedContent(
      const aura::Window* window,
      const gfx::Rect& bounds,
      base::OnceClosure stop_callback) override;
  void StopObservingRestrictedContent() override;
  void OpenFeedbackDialog() override;
  mojo::Remote<recording::mojom::RecordingService> LaunchRecordingService()
      override;
  void BindAudioStreamFactory(
      mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) override;

 private:
  std::unique_ptr<FakeRecordingService> fake_service_;
  base::FilePath fake_downloads_dir_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
