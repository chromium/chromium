// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SCREENSHOT_TAKER_H_
#define CHROME_BROWSER_FEEDBACK_SCREENSHOT_TAKER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class RenderWidgetHostView;
}

namespace feedback {

// Captures full-size screenshot to display in report-unsafe-site dialog and to
// send as part of CSD ping.
class ScreenshotTaker {
 public:
  // Callback with screenshot.
  typedef base::OnceCallback<void(const SkBitmap&)> Callback;

  // Starts taking the screenshot. The callback does not need to be set to start
  // the screenshot-taking process.
  static std::unique_ptr<ScreenshotTaker> Start(
      content::RenderWidgetHostView* render_widget_host_view);
  ~ScreenshotTaker();

  // Sets callback to run once screenshot is captured. If there is already a
  // screenshot, the callback will be called immediately.
  void SetCallback(Callback callback);

 private:
  friend class FeedbackScreenshotTakerTest;

  explicit ScreenshotTaker(
      content::RenderWidgetHostView* render_widget_host_view);

  static gfx::Size ComputeTargetSize(gfx::Size source_size, gfx::Size max_size);

  // RenderWidgetHostView::CopyFromSurface() callback.
  void OnGotScreenshot(const content::CopyFromSurfaceResult& result);

  Callback callback_;

  // The captured screenshot.
  std::optional<SkBitmap> screenshot_;

  base::WeakPtrFactory<ScreenshotTaker> weak_ptr_factory_{this};
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_SCREENSHOT_TAKER_H_
