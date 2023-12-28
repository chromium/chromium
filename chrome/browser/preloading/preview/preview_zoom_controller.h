// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_ZOOM_CONTROLLER_H_
#define CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_ZOOM_CONTROLLER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_zoom.h"

namespace content {
class WebContents;
}  // namespace content

// Preview-specific zoom controller
//
// PreviewZoomController manages per-host zoom levels, applies it to previewed
// page, and updates it via user actions. Unlike ZoomController, zoom levels are
// durable in a session and not persisted to prefs.
//
// Note that this is a short-term solution. For more details, see the discussion
// https://docs.google.com/document/d/1FS6uOrm8mxupSVVCWA5f4qwcAiZA8YsFEQwBWo0lKkk/edit?resourcekey=0-iASeiM5hicof70TpiAue6g&disco=AAAA-uh-FqQ
// TODO(b:315313138): Revisit it later.
//
// The call of ZoomController::DidFinishNavigation must be followed by
// PreviewZoomController::DidFinishNavigation to reset zoom mode and update zoom
// level for a page correctly. To ensure this, the creation of ZoomController
// must be followed by the one of PreviewZoomController, because the observer
// invocation order depends on its registration order.
class PreviewZoomController final : public content::WebContentsObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ZoomUsage {
    // A preview was started with default zoom level and not changed.
    kOnlyDefaultUsed = 0,
    // A preview was shown with non default zoom level in some timing in a
    // preview lifecycle.
    kNonDefaultUsed = 1,
    kMaxValue = kNonDefaultUsed,
  };

  explicit PreviewZoomController(content::WebContents* web_contents);
  ~PreviewZoomController() override;

  void ResetZoomForActivation();
  void Zoom(content::PageZoom zoom);

 private:
  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void InitializeZoom();

  void MaybeReportMetrics();

  bool was_page_shown_ = false;
  ZoomUsage zoom_usage_ = ZoomUsage::kOnlyDefaultUsed;
};

#endif  // CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_ZOOM_CONTROLLER_H_
