// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/lens/metrics/lens_metrics.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
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

void LensRegionSearchController::OnCaptureCompleted(
    const image_editor::ScreenshotCaptureResult& result) {
  const gfx::Image& captured_image = result.image;
  // If image is empty, then record UMA and close.
  if (captured_image.IsEmpty()) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::ERROR_CAPTURING_REGION);
    return;
  }

  const gfx::Image& image = ResizeImageIfNecessary(captured_image);
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::FAILED_TO_OPEN_TAB);
    return;
  }
  core_tab_helper->SearchWithLensInNewTab(image);
  RecordCaptureResult(lens::LensRegionSearchCaptureResult::SUCCESS);
}

}  // namespace lens
