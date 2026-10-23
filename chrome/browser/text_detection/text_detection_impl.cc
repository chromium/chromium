// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/text_detection/text_detection_impl.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/self_deleting.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace text_detection {

// static
void TextDetectionImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver) {
  if (!frame_host) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(frame_host->GetBrowserContext());
  // ScreenAI service is not available for system profiles.
  if (!profile || profile->IsSystemProfile()) {
    return;
  }
  // The object is bound to the lifetime of `frame_host`'s current document
  // and the Mojo connection via `content::DocumentService`.
  base::MakeSelfDeleting<TextDetectionImpl>(*frame_host, profile,
                                            std::move(receiver));
}

// static
TextDetectionImpl* TextDetectionImpl::CreateForTesting(
    content::RenderFrameHost* frame_host,
    scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr,
    mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver) {
  return base::MakeSelfDeleting<TextDetectionImpl>(*frame_host, std::move(ocr),
                                                   std::move(receiver));
}

TextDetectionImpl::TextDetectionImpl(
    content::RenderFrameHost& render_frame_host,
    Profile* profile,
    mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver,
    base::SelfDeletingPassKey key)
    : DocumentService(render_frame_host, std::move(receiver), key) {
  ocr_ = screen_ai::OpticalCharacterRecognizer::CreateWithStatusCallback(
      profile, screen_ai::mojom::OcrClientType::kTextDetectionApi,
      base::BindOnce(&TextDetectionImpl::OnOcrInitialized,
                     weak_factory_.GetWeakPtr()));
}

// Used only in tests via CreateForTesting().
TextDetectionImpl::TextDetectionImpl(
    content::RenderFrameHost& render_frame_host,
    scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr,
    mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver,
    base::SelfDeletingPassKey key)
    : DocumentService(render_frame_host, std::move(receiver), key),
      ocr_(std::move(ocr)) {
  CHECK(ocr_);
  if (ocr_->StatusAvailableForTesting()) {
    ocr_status_ = ocr_->is_ready() ? OcrStatus::kReady : OcrStatus::kFailed;
  }
}

TextDetectionImpl::~TextDetectionImpl() = default;

void TextDetectionImpl::SimulateOcrInitializationForTesting(bool successful) {
  OnOcrInitialized(successful);
}

void TextDetectionImpl::OnOcrInitialized(bool successful) {
  ocr_status_ = successful ? OcrStatus::kReady : OcrStatus::kFailed;

  if (pending_ensure_ready_callback_) {
    std::move(pending_ensure_ready_callback_).Run(successful);
  }

  auto requests = std::move(queued_requests_);
  for (auto& [bitmap, callback] : requests) {
    if (!successful) {
      CHECK_GT(pending_requests_, 0u);
      --pending_requests_;
      // TODO(crbug.com/570081637): The Detect() error handling spec is not
      // finalized yet, so we currently return empty results. Revisit this once
      // the spec defines the expected error type.
      std::move(callback).Run(
          std::vector<shape_detection::mojom::TextDetectionResultPtr>());
      continue;
    }
    DispatchOcrRequest(bitmap, std::move(callback));
  }
}

void TextDetectionImpl::EnsureReady(EnsureReadyCallback callback) {
  if (ensure_ready_called_) {
    // Each `TextDetector.create()` call in Blink binds a dedicated
    // `TextDetection` pipe and invokes `EnsureReady()` at most once. Receiving
    // more than one `EnsureReady()` call on the same receiver indicates a
    // compromised renderer.
    ReportBadMessageAndDeleteThis(
        "Unexpected duplicate EnsureReady() request.");
    return;
  }
  ensure_ready_called_ = true;

  auto safe_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);
  switch (ocr_status_) {
    case OcrStatus::kPending:
      pending_ensure_ready_callback_ = std::move(safe_callback);
      return;
    case OcrStatus::kReady:
      std::move(safe_callback).Run(true);
      return;
    case OcrStatus::kFailed:
      std::move(safe_callback).Run(false);
      return;
  }
  NOTREACHED();
}

void TextDetectionImpl::Detect(const SkBitmap& bitmap,
                               DetectCallback callback) {
  auto safe_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback),
      std::vector<shape_detection::mojom::TextDetectionResultPtr>());
  // TODO(crbug.com/570081637): Add a maximum bitmap dimension check
  // (`screen_ai::GetMaxDimensionForOCR()`) in follow-up CL
  // https://crrev.com/c/8535947 once Blink downsamples oversized images.
  if (bitmap.drawsNothing()) {
    std::move(safe_callback)
        .Run(std::vector<shape_detection::mojom::TextDetectionResultPtr>());
    return;
  }

  if (ocr_status_ == OcrStatus::kFailed) {
    // OCR failed to initialize or device is not compatible.
    // TODO(crbug.com/570081637): The Detect() error handling spec is not
    // finalized yet, so we currently return empty results. Revisit this once
    // the spec defines the expected error type.
    std::move(safe_callback)
        .Run(std::vector<shape_detection::mojom::TextDetectionResultPtr>());
    return;
  }

  if (pending_requests_ >= kMaxPendingRequests) {
    // TODO(crbug.com/570081637): The Detect() error handling spec is not
    // finalized yet, so we currently return empty results. Revisit this once
    // the spec defines the expected error type.
    std::move(safe_callback)
        .Run(std::vector<shape_detection::mojom::TextDetectionResultPtr>());
    return;
  }

  ++pending_requests_;
  if (ocr_status_ == OcrStatus::kPending) {
    // OCR initialization or component download is still in progress.
    queued_requests_.emplace_back(bitmap, std::move(safe_callback));
    return;
  }

  CHECK_EQ(ocr_status_, OcrStatus::kReady);
  DispatchOcrRequest(bitmap, std::move(safe_callback));
}

void TextDetectionImpl::DispatchOcrRequest(const SkBitmap& bitmap,
                                           DetectCallback callback) {
  CHECK(ocr_);
  ocr_->PerformOCR(bitmap, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                               base::BindOnce(&TextDetectionImpl::OnOcrResults,
                                              weak_factory_.GetWeakPtr(),
                                              std::move(callback)),
                               screen_ai::mojom::VisualAnnotationPtr()));
}

void TextDetectionImpl::OnOcrResults(
    DetectCallback callback,
    screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
  CHECK_GT(pending_requests_, 0u);
  --pending_requests_;

  std::vector<shape_detection::mojom::TextDetectionResultPtr> results;
  if (!visual_annotation) {
    std::move(callback).Run(std::move(results));
    return;
  }

  results.reserve(visual_annotation->lines.size());
  for (const auto& line : visual_annotation->lines) {
    if (line->text_line.empty()) {
      continue;
    }

    auto result = shape_detection::mojom::TextDetectionResult::New();
    result->raw_value = line->text_line;

    // The OCR service provides an unrotated bounding box along with a clockwise
    // rotation angle around the top-left corner (x, y). Apply this rotation
    // to obtain the actual oriented polygon corner points and the enclosing
    // axis-aligned bounding box required by the Web API.
    gfx::RectF box(line->bounding_box);
    gfx::Transform transform;
    if (std::isfinite(line->bounding_box_angle) &&
        line->bounding_box_angle != 0.0f) {
      transform.Rotate(line->bounding_box_angle);
      transform.ApplyTransformOrigin(box.x(), box.y(), 0);
    }

    // Per
    // https://wicg.github.io/shape-detection-api/text.html#detectedtext-section:
    // Enumerate corner points clockwise starting from top-left.
    result->corner_points.push_back(transform.MapPoint(box.origin()));
    result->corner_points.push_back(transform.MapPoint(box.top_right()));
    result->corner_points.push_back(transform.MapPoint(box.bottom_right()));
    result->corner_points.push_back(transform.MapPoint(box.bottom_left()));
    result->bounding_box = transform.MapRect(box);
    results.push_back(std::move(result));
  }

  std::move(callback).Run(std::move(results));
}

}  // namespace text_detection
