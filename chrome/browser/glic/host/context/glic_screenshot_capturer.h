// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"

class DesktopMediaPickerController;

namespace glic {

class GlicScreenshotCapturer : public webrtc::DesktopCapturer::Callback {
 public:
  GlicScreenshotCapturer();
  GlicScreenshotCapturer(const GlicScreenshotCapturer&) = delete;
  GlicScreenshotCapturer& operator=(const GlicScreenshotCapturer&) = delete;
  ~GlicScreenshotCapturer() override;

  // Called by GlickKeyedService to initiate a screenshot capture. Displays
  // Chrome screen picker UI for user to choose from and then runs `callback`
  // on completion to return the screenshot data. Anchors picker to
  // `parent_window`.
  void CaptureScreenshot(
      gfx::NativeWindow parent_window,
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback);
  // Called to synchronously destroy the screen picker dialog. Used when client
  // panel is closed before the screen picker flow completes.
  void CloseScreenPicker();

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
  base::WeakPtrFactory<GlicScreenshotCapturer> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_H_
