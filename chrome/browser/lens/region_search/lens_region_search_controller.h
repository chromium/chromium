// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_

#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/lens/metrics/lens_metrics.h"
#include "content/public/browser/web_contents.h"

namespace lens {

class LensRegionSearchController {
 public:
  explicit LensRegionSearchController(content::WebContents* web_contents);
  ~LensRegionSearchController();

  // Creates and runs the drag and capture flow. When run, the user enters into
  // a screenshot capture mode with the ability to draw a rectagular region
  // around the web contents. When finished with selection, the region is
  // converted into a PNG and sent to Lens.
  void Start();
  // Calculates the percentage that the image area takes up in the screen area.
  // This value is calculated as double and then floored to the nearest integer.
  static int CalculateViewportProportionFromAreas(int screen_height,
                                                  int screen_width,
                                                  int image_width,
                                                  int image_height);
  // Returns an enum representing the aspect ratio of the image as defined in
  // lens_metrics.h.
  static lens::LensRegionSearchAspectRatio GetAspectRatioFromSize(
      int image_height,
      int image_width);

 private:
  void RecordCaptureResult(lens::LensRegionSearchCaptureResult result);
  void RecordRegionSizeRelatedMetrics(gfx::Rect screen_bounds,
                                      gfx::Size region_size);

  void OnCaptureCompleted(const image_editor::ScreenshotCaptureResult& result);
  gfx::Image ResizeImageIfNecessary(const gfx::Image& image);
  content::WebContents* source_web_contents_ = nullptr;
  std::unique_ptr<image_editor::ScreenshotFlow> screenshot_flow_;

  base::WeakPtr<LensRegionSearchController> weak_this_;
  base::WeakPtrFactory<LensRegionSearchController> weak_factory_{this};
};
}  // namespace lens
#endif  // CHROME_BROWSER_LENS_REGION_SEARCH_LENS_REGION_SEARCH_CONTROLLER_H_
