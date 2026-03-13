// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site/screenshot_taker.h"

#include <algorithm>
#include <memory>

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace feedback {
namespace {

// Maximum size of screenshot to upload in CSD ping
const int kMaxUploadScreenshotWidth = 4096;
const int kMaxUploadScreenshotHeight = 2160;

void CopyFromSurface(
    content::RenderWidgetHostView* render_widget_host_view,
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const content::CopyFromSurfaceResult&)> callback) {
  if (!render_widget_host_view ||
      !render_widget_host_view->IsSurfaceAvailableForCopy() ||
      src_rect.IsEmpty() || output_size.IsEmpty()) {
    std::move(callback).Run(
        base::unexpected(content::CopyFromSurfaceError::kUnknown));
    return;
  }

  render_widget_host_view->CopyFromSurface(
      src_rect, output_size, base::TimeDelta(), std::move(callback));
}

}  // anonymous namespace

// static
std::unique_ptr<ScreenshotTaker> ScreenshotTaker::ScreenshotTaker::Start(
    content::RenderWidgetHostView* render_widget_host_view) {
  gfx::Size view_size = render_widget_host_view
                            ? render_widget_host_view->GetViewBounds().size()
                            : gfx::Size();
  auto screenshot_taker = base::WrapUnique<ScreenshotTaker>(new ScreenshotTaker(
      base::BindOnce(&CopyFromSurface, render_widget_host_view), view_size));
  return screenshot_taker;
}

ScreenshotTaker::~ScreenshotTaker() = default;

void ScreenshotTaker::SetCallback(Callback callback) {
  callback_ = std::move(callback);
  if (screenshot_) {
    std::move(callback_).Run(screenshot_.value());
  }
}

ScreenshotTaker::ScreenshotTaker(
    CopyFromSurfaceCallback copy_from_surface_callback,
    gfx::Size view_size) {
  gfx::Size target_size = ComputeTargetSize(
      view_size,
      gfx::Size(kMaxUploadScreenshotWidth, kMaxUploadScreenshotHeight));
  std::move(copy_from_surface_callback)
      .Run(gfx::Rect(view_size), target_size,
           base::BindOnce(&ScreenshotTaker::OnGotScreenshot,
                          weak_ptr_factory_.GetWeakPtr()));
}

// static
gfx::Size ScreenshotTaker::ComputeTargetSize(gfx::Size source_size,
                                             gfx::Size max_size) {
  float largest_ratio =
      std::max({((float)source_size.width()) / max_size.width(),
                ((float)source_size.height()) / max_size.height(), 1.0f});
  return gfx::Size(static_cast<int>(source_size.width() / largest_ratio),
                   static_cast<int>(source_size.height() / largest_ratio));
}

void ScreenshotTaker::OnGotScreenshot(
    const content::CopyFromSurfaceResult& result) {
  screenshot_ = result.has_value() ? result.value().bitmap : SkBitmap();
  if (callback_) {
    std::move(callback_).Run(screenshot_.value());
  }
}

}  // namespace feedback
