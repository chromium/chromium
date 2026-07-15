// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_IMPL_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"

class DesktopMediaPickerController;

namespace glic {

class GlicScreenshotCapturerImpl : public GlicScreenshotCapturer,
                                   public webrtc::DesktopCapturer::Callback {
 public:
  GlicScreenshotCapturerImpl();
  GlicScreenshotCapturerImpl(const GlicScreenshotCapturerImpl&) = delete;
  GlicScreenshotCapturerImpl& operator=(const GlicScreenshotCapturerImpl&) =
      delete;
  ~GlicScreenshotCapturerImpl() override;

  // GlicScreenshotCapturer:
  void CaptureScreenshot(
      gfx::NativeWindow parent_window,
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback)
      override;
  void CloseScreenPicker() override;

  // Exposes the internal ConvertFrameToJpeg() function to unit tests.
  static std::vector<uint8_t> ConvertFrameToJpegForTesting(
      std::unique_ptr<webrtc::DesktopFrame> frame);

 private:
  // Callback triggered when user selects a source to capture.
  void OnSourceSelected(const std::string& err, content::DesktopMediaID id);

  // Called to start the actual screen capture.
  void OnCaptureStarted(content::DesktopMediaID id);

  // webrtc::DesktopCapturer::Callback:
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Signal captured screenshot back to the client.
  void SignalScreenshotResult(std::vector<uint8_t> jpeg_data);
  // Signal an error back to the client.
  void SignalError(glic::mojom::CaptureScreenshotErrorReason error_reason);

  glic::mojom::WebClientHandler::CaptureScreenshotCallback capture_callback_;
  webrtc::DesktopSize frame_size_;
  std::unique_ptr<DesktopMediaPickerController> picker_controller_;
  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;
  base::WeakPtrFactory<GlicScreenshotCapturerImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_IMPL_H_
