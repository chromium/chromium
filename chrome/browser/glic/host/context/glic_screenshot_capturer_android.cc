// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_screenshot_capturer_android.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ui/android/window_android.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"
#include "ui/snapshot/snapshot.h"

namespace glic {

GlicScreenshotCapturerAndroid::GlicScreenshotCapturerAndroid() = default;

GlicScreenshotCapturerAndroid::~GlicScreenshotCapturerAndroid() {
  if (capture_callback_) {
    std::move(capture_callback_)
        .Run(mojom::CaptureScreenshotResult::NewErrorReason(
            mojom::CaptureScreenshotErrorReason::kUnknown));
  }
}

void GlicScreenshotCapturerAndroid::CaptureScreenshot(
    gfx::NativeWindow parent_window,
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  if (capture_callback_) {
    std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
        mojom::CaptureScreenshotErrorReason::kScreenCaptureRequestThrottled));
    return;
  }
  if (!parent_window) {
    std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
        mojom::CaptureScreenshotErrorReason::kUnknown));
    return;
  }

  capture_callback_ = std::move(callback);
  ui::GrabWindowSnapshot(
      parent_window, gfx::Rect(parent_window->GetPhysicalBackingSize()),
      base::BindOnce(&GlicScreenshotCapturerAndroid::OnSnapshotCaptured,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicScreenshotCapturerAndroid::OnSnapshotCaptured(gfx::Image snapshot) {
  if (!capture_callback_) {
    return;
  }
  if (snapshot.IsEmpty()) {
    std::move(capture_callback_)
        .Run(mojom::CaptureScreenshotResult::NewErrorReason(
            mojom::CaptureScreenshotErrorReason::kUnknown));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&gfx::JPEG1xEncodedDataFromImage, snapshot,
                     /*quality=*/100),
      base::BindOnce(&GlicScreenshotCapturerAndroid::OnScreenshotEncoded,
                     weak_ptr_factory_.GetWeakPtr(), snapshot.Width(),
                     snapshot.Height()));
}

void GlicScreenshotCapturerAndroid::OnScreenshotEncoded(
    int width,
    int height,
    std::optional<std::vector<uint8_t>> jpeg_data) {
  if (!capture_callback_) {
    return;
  }
  if (!jpeg_data || jpeg_data->empty()) {
    std::move(capture_callback_)
        .Run(mojom::CaptureScreenshotResult::NewErrorReason(
            mojom::CaptureScreenshotErrorReason::kUnknown));
    return;
  }

  mojom::ScreenshotPtr mojo_screenshot = mojom::Screenshot::New();
  mojo_screenshot->width_pixels = width;
  mojo_screenshot->height_pixels = height;
  mojo_screenshot->mime_type = "image/jpeg";
  mojo_screenshot->data = std::move(*jpeg_data);
  mojo_screenshot->origin_annotations = mojom::ImageOriginAnnotations::New();

  std::move(capture_callback_)
      .Run(mojom::CaptureScreenshotResult::NewScreenshot(
          std::move(mojo_screenshot)));
}

void GlicScreenshotCapturerAndroid::CloseScreenPicker() {
  // No-op on Android since there is no desktop screen picker.
}

}  // namespace glic
