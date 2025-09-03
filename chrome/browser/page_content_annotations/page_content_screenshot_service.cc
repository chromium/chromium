// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_screenshot_service.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace page_content_annotations {

using BitmapCallback = PageContentScreenshotService::BitmapCallback;
namespace {

paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
PrepareCompositeRequest(
    std::unique_ptr<paint_preview::CaptureResult> capture_result) {
  auto begin_composite_request =
      paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
  auto [map, proto] =
      paint_preview::RecordingMapFromCaptureResult(std::move(*capture_result));
  begin_composite_request->recording_map = std::move(map);
  if (begin_composite_request->recording_map.empty()) {
    VLOG(2) << "Captured an empty screenshot";
    return nullptr;
  }
  begin_composite_request->preview = mojo_base::ProtoWrapper(std::move(proto));
  return begin_composite_request;
}

}  // namespace

class ScreenshotRequest {
 public:
  ScreenshotRequest(
      base::WeakPtr<PageContentScreenshotService> screenshot_service,
      PageContentScreenshotService::RequestParams params)
      : screenshot_service_(screenshot_service), params_(std::move(params)) {}

  ScreenshotRequest() = delete;

  ScreenshotRequest(const ScreenshotRequest&) = delete;
  ScreenshotRequest(ScreenshotRequest&&) = delete;
  ScreenshotRequest& operator=(const ScreenshotRequest&) = delete;
  ScreenshotRequest& operator=(ScreenshotRequest&&) = delete;

  ~ScreenshotRequest() = default;

  void TakeScreenshot(content::WebContents* web_contents,
                      BitmapCallback callback) {
    CHECK(callback);
    callback_ = std::move(callback);

    paint_preview::PaintPreviewBaseService::CaptureParams capture_params;
    capture_params.web_contents = web_contents;
    capture_params.clip_rect = params_.clip_rect;
    capture_params.clip_x_coord_override = params_.clip_x_coord_override;
    capture_params.clip_y_coord_override = params_.clip_y_coord_override;
    capture_params.persistence =
        paint_preview::RecordingPersistence::kMemoryBuffer;
    capture_params.max_per_capture_size = params_.max_per_capture_bytes;
    capture_params.redaction_params = std::move(params_.redaction_params);

    screenshot_service_->CapturePaintPreview(
        capture_params, base::BindOnce(&ScreenshotRequest::OnScreenshotCaptured,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnScreenshotCaptured(
      paint_preview::PaintPreviewBaseService::CaptureStatus status,
      std::unique_ptr<paint_preview::CaptureResult> result) {
    if (!screenshot_service_) {
      std::move(callback_).Run(base::unexpected("Service deallocated"));
      return;
    }

    if (status != paint_preview::PaintPreviewBaseService::CaptureStatus::kOk ||
        !result->capture_success) {
      std::move(callback_).Run(
          base::unexpected(paint_preview::ToString(status)));
      return;
    }
    paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
        composite_request = PrepareCompositeRequest(std::move(result));

    if (!screenshot_service_->compositor_client(PassKey())) {
      screenshot_service_->CreateCompositorClient(
          base::PassKey<ScreenshotRequest>(),
          base::BindOnce(&ScreenshotRequest::SendBeginCompositeRequest,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(composite_request)));
      return;
    }

    SendBeginCompositeRequest(std::move(composite_request));
  }

  void SendBeginCompositeRequest(
      paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
          begin_composite_request) {
    if (!screenshot_service_) {
      std::move(callback_).Run(base::unexpected("Service Deallocated"));
      return;
    }
    if (!begin_composite_request) {
      std::move(callback_).Run(
          base::unexpected("Invalid begin_composite_request"));
      return;
    }

    CHECK(screenshot_service_->compositor_client(PassKey()));
    screenshot_service_->compositor_client(PassKey())->BeginMainFrameComposite(
        std::move(begin_composite_request),
        base::BindOnce(&ScreenshotRequest::OnBeginCompositeFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnBeginCompositeFinished(
      paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
      paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr response) {
    using enum paint_preview::mojom::PaintPreviewCompositor::
        BeginCompositeStatus;
    if (status != kSuccess && status != kPartialSuccess) {
      std::move(callback_).Run(base::unexpected(base::ToString(status)));
      return;
    }

    // Start converting to a bitmap.
    if (!screenshot_service_) {
      std::move(callback_).Run(base::unexpected("Service deallocated"));
      return;
    }
    screenshot_service_->compositor_client(PassKey())->BitmapForMainFrame(
        params_.clip_rect, params_.scale_factor,
        base::BindOnce(&ScreenshotRequest::OnBitmapReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnBitmapReceived(
      paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
      const SkBitmap& bitmap) {
    if (status != paint_preview::mojom::PaintPreviewCompositor::BitmapStatus::
                      kSuccess ||
        bitmap.empty()) {
      std::move(callback_).Run(base::unexpected(base::ToString(status)));
      return;
    }

    std::move(callback_).Run(&bitmap);
  }

  base::PassKey<ScreenshotRequest> PassKey() const {
    return base::PassKey<ScreenshotRequest>();
  }

  base::WeakPtr<ScreenshotRequest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtr<PageContentScreenshotService> screenshot_service_;
  PageContentScreenshotService::RequestParams params_;
  BitmapCallback callback_;
  base::WeakPtrFactory<ScreenshotRequest> weak_ptr_factory_{this};
};

PageContentScreenshotService::PageContentScreenshotService()
    : paint_preview::PaintPreviewBaseService(
          /*file_mixin=*/nullptr,  // in-memory captures
          /*policy=*/nullptr,      // all content is deemed amenable
          /*is_off_the_record=*/false),
      compositor_service_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      compositor_client_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  compositor_service_ = CreateCompositorService();
}

PageContentScreenshotService::~PageContentScreenshotService() = default;

void PageContentScreenshotService::RequestScreenshot(
    content::WebContents* web_contents,
    RequestParams params,
    BitmapCallback callback) {
  CHECK(callback);
  if (!web_contents) {
    std::move(callback).Run(
        base::unexpected("The given web contents is no longer valid"));
    return;
  }

  auto request = std::make_unique<ScreenshotRequest>(
      weak_ptr_factory_.GetWeakPtr(), std::move(params));

  auto* raw_request = request.get();
  raw_request->TakeScreenshot(
      web_contents,
      base::BindOnce(
          // Bind `request` to the callback to keep it in scope
          // until the callback returns.
          [](std::unique_ptr<ScreenshotRequest> request,
             BitmapCallback callback,
             base::expected<const SkBitmap*, std::string> result) {
            std::move(callback).Run(result);
          },
          std::move(request), std::move(callback)));
}

std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                base::OnTaskRunnerDeleter>
PageContentScreenshotService::CreateCompositorService() {
  std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                  base::OnTaskRunnerDeleter>
      service = paint_preview::StartCompositorService(base::BindOnce(
          &PageContentScreenshotService::OnCompositorServiceDisconnected,
          weak_ptr_factory_.GetWeakPtr()));
  CHECK(service);
  return service;
}

void PageContentScreenshotService::CreateCompositorClient(
    base::PassKey<ScreenshotRequest>,
    base::OnceClosure callback) {
  if (!compositor_service_) {
    compositor_service_ = CreateCompositorService();
  }
  compositor_client_ =
      compositor_service_->CreateCompositor(std::move(callback));
}

void PageContentScreenshotService::OnCompositorServiceDisconnected() {
  VLOG(2) << "Compositor service is disconnected";
  compositor_client_.reset();
  compositor_service_.reset();
}

}  // namespace page_content_annotations
