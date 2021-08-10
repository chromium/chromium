// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/lens/metrics/lens_metrics.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image_util.h"

namespace lens {

LensRegionSearchController::LensRegionSearchController(
    content::WebContents* web_contents)
    : source_web_contents_(web_contents) {
  screenshot_flow_ =
      std::make_unique<image_editor::ScreenshotFlow>(web_contents);
  weak_this_ = weak_factory_.GetWeakPtr();
}

LensRegionSearchController::~LensRegionSearchController() = default;

void LensRegionSearchController::Start() {
  if (!source_web_contents_)
    return;

  if (!screenshot_flow_)
    screenshot_flow_ =
        std::make_unique<image_editor::ScreenshotFlow>(source_web_contents_);

  base::OnceCallback<void(const image_editor::ScreenshotCaptureResult&)>
      callback = base::BindOnce(&LensRegionSearchController::OnCaptureCompleted,
                                weak_this_);
  screenshot_flow_->Start(std::move(callback));
}

gfx::Image LensRegionSearchController::ResizeImageIfNecessary(
    const gfx::Image& image) {
  return gfx::ResizedImageForMaxDimensions(
      image, features::GetMaxPixelsForRegionSearch(),
      features::GetMaxPixelsForRegionSearch(),
      features::GetMaxAreaForRegionSearch());
}

void LensRegionSearchController::RecordCaptureResult(
    lens::LensRegionSearchCaptureResult result) {
  UMA_HISTOGRAM_ENUMERATION(lens::kLensRegionSearchCaptureResultHistogramName,
                            result);
}

int LensRegionSearchController::CalculateViewportProportionFromAreas(
    int screen_height,
    int screen_width,
    int image_width,
    int image_height) {
  // To get the region proportion of the screen, we must calculate the areas of
  // the screen and captured region. Then, we must divide the area of the region
  // by the area of the screen to get the percentage. Multiply by 100 to make it
  // an integer. Returns -1 if screen_area is 0 to prevent undefined values.
  double screen_area = screen_width * screen_height;
  if (screen_area <= 0) {
    return -1;
  }
  double region_area = image_width * image_height;
  double region_proportion = region_area / screen_area;
  int region_proportion_int = region_proportion * 100;
  return region_proportion_int;
}

lens::LensRegionSearchAspectRatio
LensRegionSearchController::GetAspectRatioFromSize(int image_height,
                                                   int image_width) {
  // Convert to double to prevent integer division.
  double width = image_width;
  double height = image_height;

  // To record region aspect ratio, we must divide the region's width by height.
  // Should never be zero, but check to prevent any crashes or undefined
  // recordings.
  if (height > 0) {
    double aspect_ratio = width / height;
    if (aspect_ratio <= 1.2 && aspect_ratio >= 0.8) {
      return lens::LensRegionSearchAspectRatio::SQUARE;
    } else if (aspect_ratio < 0.8 && aspect_ratio >= 0.3) {
      return lens::LensRegionSearchAspectRatio::TALL;
    } else if (aspect_ratio < 0.3) {
      return lens::LensRegionSearchAspectRatio::VERY_TALL;
    } else if (aspect_ratio > 1.2 && aspect_ratio <= 1.7) {
      return lens::LensRegionSearchAspectRatio::WIDE;
    } else if (aspect_ratio > 1.7) {
      return lens::LensRegionSearchAspectRatio::VERY_WIDE;
    }
  }
  return lens::LensRegionSearchAspectRatio::UNDEFINED;
}

void LensRegionSearchController::RecordRegionSizeRelatedMetrics(
    gfx::Rect screen_bounds,
    gfx::Size image_size) {
  // If any of the rects are empty, it means the area is zero. In this case,
  // return.
  if (screen_bounds.IsEmpty() || image_size.IsEmpty())
    return;
  double region_width = image_size.width();
  double region_height = image_size.height();

  int region_proportion = CalculateViewportProportionFromAreas(
      screen_bounds.height(), screen_bounds.width(), region_width,
      region_height);
  if (region_proportion >= 0) {
    base::UmaHistogramPercentage(
        lens::kLensRegionSearchRegionViewportProportionHistogramName,
        region_proportion);
  }

  // To record region aspect ratio, we must divide the region's width by height.
  base::UmaHistogramEnumeration(
      lens::kLensRegionSearchRegionAspectRatioHistogramName,
      GetAspectRatioFromSize(region_height, region_width));
}

void LensRegionSearchController::OnCaptureCompleted(
    const image_editor::ScreenshotCaptureResult& result) {
  const gfx::Image& captured_image = result.image;
  // If image is empty, then record UMA and close.
  if (captured_image.IsEmpty()) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::ERROR_CAPTURING_REGION);
    return;
  }

  // Record region size related UMA histograms according to region and screen.
  RecordRegionSizeRelatedMetrics(result.screen_bounds, captured_image.Size());

  const gfx::Image& image = ResizeImageIfNecessary(captured_image);
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::FAILED_TO_OPEN_TAB);
    return;
  }
  core_tab_helper->SearchWithLensInNewTab(
      image, captured_image.Size(),
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM);
  RecordCaptureResult(lens::LensRegionSearchCaptureResult::SUCCESS);
}

}  // namespace lens
