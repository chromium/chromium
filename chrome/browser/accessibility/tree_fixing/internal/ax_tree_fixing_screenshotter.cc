// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screenshotter.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/recording_map.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/geometry/rect.h"

constexpr size_t kMaxPerCaptureSizeBytes = 50 * 1000L * 1000L;  // 50 MB.

AXTreeFixingScreenshotter::AXTreeFixingScreenshotter(
    ScreenshotDelegate& delegate)
    : paint_preview::PaintPreviewBaseService(
          /*file_mixin=*/nullptr,  // in-memory captures
          /*policy=*/nullptr,      // all content is deemed amenable
          /*is_off_the_record=*/false),
      screenshot_delegate_(delegate),
      compositor_service_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      compositor_client_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  compositor_service_ = paint_preview::StartCompositorService(base::BindOnce(
      &AXTreeFixingScreenshotter::OnCompositorServiceDisconnected,
      weak_ptr_factory_.GetWeakPtr()));
  CHECK(compositor_service_);
}

AXTreeFixingScreenshotter::~AXTreeFixingScreenshotter() = default;

void AXTreeFixingScreenshotter::RequestScreenshot(
    const raw_ptr<content::WebContents> web_contents,
    int request_id) {
  if (!web_contents) {
    return;
  }

  CaptureParams capture_params;
  capture_params.web_contents = web_contents;
  capture_params.persistence =
      paint_preview::RecordingPersistence::kMemoryBuffer;
  capture_params.max_per_capture_size = kMaxPerCaptureSizeBytes;
  CapturePaintPreview(
      capture_params,
      base::BindOnce(&AXTreeFixingScreenshotter::OnPaintPreviewCaptured,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void AXTreeFixingScreenshotter::OnCompositorServiceDisconnected() {
  compositor_client_.reset();
  compositor_service_.reset();
}

void AXTreeFixingScreenshotter::OnPaintPreviewCaptured(
    int request_id,
    paint_preview::PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<paint_preview::CaptureResult> result) {
  if (status != PaintPreviewBaseService::CaptureStatus::kOk ||
      !result->capture_success) {
    return;
  }
  if (!compositor_client_) {
    compositor_client_ = compositor_service_->CreateCompositor(base::BindOnce(
        &AXTreeFixingScreenshotter::SendCompositeRequest,
        weak_ptr_factory_.GetWeakPtr(), request_id, std::move(result)));
    return;
  }
  SendCompositeRequest(request_id, std::move(result));
}

void AXTreeFixingScreenshotter::SendCompositeRequest(
    int request_id,
    std::unique_ptr<paint_preview::CaptureResult> result) {
  paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr request =
      paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
  std::pair<paint_preview::RecordingMap, paint_preview::PaintPreviewProto>
      map_and_proto =
          paint_preview::RecordingMapFromCaptureResult(std::move(*result));
  request->recording_map = std::move(map_and_proto.first);
  request->preview = mojo_base::ProtoWrapper(std::move(map_and_proto.second));
  compositor_client_->BeginMainFrameComposite(
      std::move(request),
      base::BindOnce(&AXTreeFixingScreenshotter::OnCompositeFinished,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void AXTreeFixingScreenshotter::OnCompositeFinished(
    int request_id,
    paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
    paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr response) {
  if (status != paint_preview::mojom::PaintPreviewCompositor::
                    BeginCompositeStatus::kSuccess &&
      status != paint_preview::mojom::PaintPreviewCompositor::
                    BeginCompositeStatus::kPartialSuccess) {
    return;
  }

  // Pass an empty `gfx::Rect` allows us to get a bitmap for the full page.
  compositor_client_->BitmapForMainFrame(
      gfx::Rect(), /*scale_factor=*/1.0,
      base::BindOnce(&AXTreeFixingScreenshotter::OnBitmapReceived,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void AXTreeFixingScreenshotter::OnBitmapReceived(
    int request_id,
    paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
    const SkBitmap& bitmap) {
  if (status != paint_preview::mojom::PaintPreviewCompositor::BitmapStatus::
                    kSuccess ||
      bitmap.empty()) {
    return;
  }

  screenshot_delegate_->OnScreenshotCaptured(bitmap, request_id);
}
