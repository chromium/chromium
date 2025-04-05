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
  // Delegate for clients that want to screenshot WebContents.
  class ScreenshotDelegate {
   protected:
    ScreenshotDelegate() = default;

   public:
    ScreenshotDelegate(const ScreenshotDelegate&) = delete;
    ScreenshotDelegate& operator=(const ScreenshotDelegate&) = delete;
    virtual ~ScreenshotDelegate() = default;

    // This method is used to return the captured screenshot to the delegate
    // (owner) of this instance. When calling RequestScreenshot, the client must
    // provide a request_id, and this ID is passed back to the client. The
    // request_id allows clients to make multiple requests in parallel and
    // uniquely identify each response. It is the responsibility of the client
    // to handle the logic behind a request_id, this service simply passes the
    // id through.
    virtual void OnScreenshotCaptured(const SkBitmap& bitmap,
                                      int request_id) = 0;
  };

  explicit AXTreeFixingScreenshotter(ScreenshotDelegate& delegate);
  AXTreeFixingScreenshotter(const AXTreeFixingScreenshotter&) = delete;
  AXTreeFixingScreenshotter& operator=(const AXTreeFixingScreenshotter&) =
      delete;
  ~AXTreeFixingScreenshotter() override;

  // Initiates the process of capturing a paint preview screenshot for the
  // given |web_contents|. On completion, OnPaintPreviewCaptured() will be
  // invoked. Does nothing if |web_contents| is null.
  // The client should provide a request_id, which is returned to the client
  // along with the resulting bitmap via a call to OnScreenshotCaptured.
  void RequestScreenshot(const raw_ptr<content::WebContents> web_contents,
                         int request_id);

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
  void OnPaintPreviewCaptured(
      int request_id,
      paint_preview::PaintPreviewBaseService::CaptureStatus status,
      std::unique_ptr<paint_preview::CaptureResult> result);

  // Sends the captured paint preview data (|result|) to the compositor service
  // to begin compositing into a bitmap.
  // OnCompositeFinished() will be called upon completion of the compositing.
  void SendCompositeRequest(
      int request_id,
      std::unique_ptr<paint_preview::CaptureResult> result);

  // Callback invoked when the compositor service finishes the request initiated
  // by SendCompositeRequest(). If compositing was successful (or partially
  // successful), it requests the final bitmap of the main frame.
  void OnCompositeFinished(
      int request_id,
      paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
      paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr response);

  // Callback invoked when the compositor service provides the requested bitmap.
  // This is the final step in the screenshot pipeline. This returns the
  // resulting bitmap to the owner of this instance via the provided delegate.
  void OnBitmapReceived(
      int request_id,
      paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
      const SkBitmap& bitmap);

  // Delegate provided by client to receive screenshot.
  // Use a raw_ref since we do not own the delegate or control its lifecycle.
  const raw_ref<ScreenshotDelegate> screenshot_delegate_;

  std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                  base::OnTaskRunnerDeleter>
      compositor_service_;
  std::unique_ptr<paint_preview::PaintPreviewCompositorClient,
                  base::OnTaskRunnerDeleter>
      compositor_client_;

  base::WeakPtrFactory<AXTreeFixingScreenshotter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_INTERNAL_AX_TREE_FIXING_SCREENSHOTTER_H_
