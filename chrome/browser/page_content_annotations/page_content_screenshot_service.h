// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_SCREENSHOT_SERVICE_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_SCREENSHOT_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom.h"
#include "components/paint_preview/common/redaction_params.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "content/public/browser/web_contents.h"

class SkBitmap;

namespace page_content_annotations {

class ScreenshotRequest;

// This service takes a page/viewport screenshot using Paint Preview.
class PageContentScreenshotService
    : public paint_preview::PaintPreviewBaseService {
 public:
  using BitmapCallback =
      base::OnceCallback<void(base::expected<const SkBitmap*, std::string>)>;

  PageContentScreenshotService();
  PageContentScreenshotService(const PageContentScreenshotService&) = delete;
  PageContentScreenshotService& operator=(const PageContentScreenshotService&) =
      delete;
  ~PageContentScreenshotService() override;

  struct RequestParams {
    gfx::Rect clip_rect;
    double scale_factor;
    paint_preview::mojom::ClipCoordOverride clip_x_coord_override =
        paint_preview::mojom::ClipCoordOverride::kNone;
    paint_preview::mojom::ClipCoordOverride clip_y_coord_override =
        paint_preview::mojom::ClipCoordOverride::kNone;
    paint_preview::RedactionParams redaction_params;
    // Maximum number of bytes for a single frame capture. 0 means "no limit".
    size_t max_per_capture_bytes;
  };

  void RequestScreenshot(content::WebContents* web_contents,
                         RequestParams params,
                         BitmapCallback callback);

  paint_preview::PaintPreviewCompositorService* compositor_service(
      base::PassKey<ScreenshotRequest>) const {
    return compositor_service_.get();
  }

  paint_preview::PaintPreviewCompositorClient* compositor_client(
      base::PassKey<ScreenshotRequest>) const {
    return compositor_client_.get();
  }

  void CreateCompositorClient(base::PassKey<ScreenshotRequest>,
                              base::OnceClosure callback);

 private:
  std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                  base::OnTaskRunnerDeleter>
  CreateCompositorService();

  void OnCompositorServiceDisconnected();

  std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                  base::OnTaskRunnerDeleter>
      compositor_service_;
  std::unique_ptr<paint_preview::PaintPreviewCompositorClient,
                  base::OnTaskRunnerDeleter>
      compositor_client_;

  base::WeakPtrFactory<PageContentScreenshotService> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_SCREENSHOT_SERVICE_H_
