// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_H_

#include <memory>

#include "chrome/browser/glic/host/glic.mojom.h"
#include "ui/gfx/native_ui_types.h"

namespace glic {

class GlicScreenshotCapturer {
 public:
  static std::unique_ptr<GlicScreenshotCapturer> Create();
  virtual ~GlicScreenshotCapturer() = default;

  virtual void CaptureScreenshot(
      gfx::NativeWindow parent_window,
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) = 0;
  virtual void CloseScreenPicker() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SCREENSHOT_CAPTURER_H_
