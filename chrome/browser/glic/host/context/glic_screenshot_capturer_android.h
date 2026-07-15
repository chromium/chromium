// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_ANDROID_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_ANDROID_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "ui/gfx/image/image.h"

namespace glic {

class GlicScreenshotCapturerAndroid : public GlicScreenshotCapturer {
 public:
  GlicScreenshotCapturerAndroid();
  GlicScreenshotCapturerAndroid(const GlicScreenshotCapturerAndroid&) = delete;
  GlicScreenshotCapturerAndroid& operator=(
      const GlicScreenshotCapturerAndroid&) = delete;
  ~GlicScreenshotCapturerAndroid() override;

  // GlicScreenshotCapturer:
  void CaptureScreenshot(
      gfx::NativeWindow parent_window,
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback)
      override;
  void CloseScreenPicker() override;

 private:
  void OnSnapshotCaptured(gfx::Image snapshot);
  void OnScreenshotEncoded(int width,
                           int height,
                           std::optional<std::vector<uint8_t>> jpeg_data);

  glic::mojom::WebClientHandler::CaptureScreenshotCallback capture_callback_;
  base::WeakPtrFactory<GlicScreenshotCapturerAndroid> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_ANDROID_H_
