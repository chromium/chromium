// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TEXT_DETECTION_TEXT_DETECTION_IMPL_H_
#define CHROME_BROWSER_TEXT_DETECTION_TEXT_DETECTION_IMPL_H_

#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/self_deleting.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace content {
class RenderFrameHost;
}

namespace screen_ai {
class OpticalCharacterRecognizer;
}

namespace text_detection {

// Implements shape_detection::mojom::TextDetection using the browser OCR
// engine. Currently, this implementation is only registered in
// `PopulateChromeFrameBinders()` for `RenderFrameHost` (`Window` contexts) on
// desktop platforms. Worker contexts (`DedicatedWorker`, `SharedWorker`, and
// `ServiceWorker`) still bind through `content`'s default binders, falling back
// to the OS-native `ShapeDetectionService` implementation (or closing the pipe
// if unsupported).
// TODO(crbug.com/40276465): Evaluate which worker contexts should use the
// browser OCR engine and register the corresponding worker interface binders.
class TextDetectionImpl
    : public content::DocumentService<shape_detection::mojom::TextDetection> {
 public:
  // Temporary cap on total pending and in-flight Detect() requests (both during
  // and after OCR initialization) to avoid unbounded memory growth / overflow
  // attacks.
  static constexpr size_t kMaxPendingRequests = 10;

  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver);

  static TextDetectionImpl* CreateForTesting(
      content::RenderFrameHost* frame_host,
      scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr,
      mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver);

  TextDetectionImpl(
      content::RenderFrameHost& render_frame_host,
      Profile* profile,
      mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver,
      base::SelfDeletingPassKey key);

  // Used only in tests via CreateForTesting() to inject a fake
  // OpticalCharacterRecognizer.
  TextDetectionImpl(
      content::RenderFrameHost& render_frame_host,
      scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr,
      mojo::PendingReceiver<shape_detection::mojom::TextDetection> receiver,
      base::SelfDeletingPassKey key);

  TextDetectionImpl(const TextDetectionImpl&) = delete;
  TextDetectionImpl& operator=(const TextDetectionImpl&) = delete;

  // shape_detection::mojom::TextDetection:
  void Detect(const SkBitmap& bitmap, DetectCallback callback) override;
  void EnsureReady(EnsureReadyCallback callback) override;

  void SimulateOcrInitializationForTesting(bool successful);

 private:
  enum class OcrStatus {
    kPending,
    kReady,
    kFailed,
  };

  ~TextDetectionImpl() override;

  void DispatchOcrRequest(const SkBitmap& bitmap, DetectCallback callback);
  void OnOcrInitialized(bool successful);
  void OnOcrResults(DetectCallback callback,
                    screen_ai::mojom::VisualAnnotationPtr visual_annotation);

  scoped_refptr<screen_ai::OpticalCharacterRecognizer> ocr_;
  OcrStatus ocr_status_ = OcrStatus::kPending;

  // Total number of unfinished Detect() requests (including both those queued
  // in `queued_requests_` waiting for OCR initialization and those actively in
  // flight in `ocr_->PerformOCR()`).
  size_t pending_requests_ = 0;

  // Requests queued while waiting for OCR initialization to complete.
  std::vector<std::pair<SkBitmap, DetectCallback>> queued_requests_;
  bool ensure_ready_called_ = false;
  EnsureReadyCallback pending_ensure_ready_callback_;

  base::WeakPtrFactory<TextDetectionImpl> weak_factory_{this};
};

}  // namespace text_detection

#endif  // CHROME_BROWSER_TEXT_DETECTION_TEXT_DETECTION_IMPL_H_
