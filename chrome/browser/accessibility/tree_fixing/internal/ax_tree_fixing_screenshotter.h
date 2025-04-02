// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_SCREENSHOTTER_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_SCREENSHOTTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "content/public/browser/web_contents.h"

// Used to capture a full-page screenshot of a WebContents.
class AXTreeFixingScreenshotter
    : public paint_preview::PaintPreviewBaseService {
 public:
  AXTreeFixingScreenshotter();
  AXTreeFixingScreenshotter(const AXTreeFixingScreenshotter&) = delete;
  AXTreeFixingScreenshotter& operator=(const AXTreeFixingScreenshotter&) =
      delete;
  ~AXTreeFixingScreenshotter() override;

  // Initiates the process of capturing a paint preview screenshot for the
  // given |web_contents|. On completion, OnScreenshotCaptured() will be
  // invoked. Does nothing if |web_contents| is null.
  void RequestScreenshot(const raw_ptr<content::WebContents> web_contents);

 private:
  // Called when the connection to the Paint Preview Compositor Service is lost.
  // Resets the compositor client and service pointers to ensure they are not
  // used in a disconnected state.
  void OnCompositorServiceDisconnected();

  // Callback invoked when the capture initiated by RequestScreenshot()
  // completes. If the capture was successful, it proceeds to the compositing
  // stage by calling SendCompositeRequest(). If this is the first successful
  // capture, it will also establish the connection to the compositor service
  // via CreateCompositor.
  void OnScreenshotCaptured(
      paint_preview::PaintPreviewBaseService::CaptureStatus status,
      std::unique_ptr<paint_preview::CaptureResult> result);

  // Sends the captured paint preview data (|result|) to the compositor service
  // to begin compositing into a bitmap.
  // OnCompositeFinished() will be called upon completion of the compositing.
  void SendCompositeRequest(
      std::unique_ptr<paint_preview::CaptureResult> result);

  // Callback invoked when the compositor service finishes the request initiated
  // by SendCompositeRequest(). If compositing was successful (or partially
  // successful), it requests the final bitmap of the main frame.
  void OnCompositeFinished(
      paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
      paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr response);

  // Callback invoked when the compositor service provides the requested bitmap.
  // This is the final step in the screenshot pipeline.
  void OnBitmapReceived(
      paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
      const SkBitmap& bitmap);

  std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                  base::OnTaskRunnerDeleter>
      compositor_service_;
  std::unique_ptr<paint_preview::PaintPreviewCompositorClient,
                  base::OnTaskRunnerDeleter>
      compositor_client_;

  base::WeakPtrFactory<AXTreeFixingScreenshotter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_SCREENSHOTTER_H_
