// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/region_search/lens_region_search_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_metrics.h"
#include "components/lens/lens_rendering_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/views/lens/lens_side_panel_helper.h"
#endif

namespace lens {

LensRegionSearchControllerData::LensRegionSearchControllerData() = default;
LensRegionSearchControllerData::~LensRegionSearchControllerData() = default;

RegionSearchCapturedData::RegionSearchCapturedData() = default;
RegionSearchCapturedData::~RegionSearchCapturedData() = default;

LensRegionSearchController::LensRegionSearchController() {
  weak_this_ = weak_factory_.GetWeakPtr();
}

LensRegionSearchController::~LensRegionSearchController() {
  CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void LensRegionSearchController::Start(
    content::WebContents* web_contents,
    bool use_fullscreen_capture,
    bool force_open_in_new_tab,
    bool is_google_default_search_provider,
    lens::AmbientSearchEntryPoint entry_point) {
  entry_point_ = entry_point;
  force_open_in_new_tab_ = force_open_in_new_tab;
  is_google_default_search_provider_ = is_google_default_search_provider;
  // Return early if web contents/browser don't exist and if capture mode is
  // already active.
  if (!web_contents || in_capture_mode_) {
    return;
  }
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }

  Observe(web_contents);
  if (!screenshot_flow_)
    screenshot_flow_ =
        std::make_unique<image_editor::ScreenshotFlow>(web_contents);

  base::OnceCallback<void(const image_editor::ScreenshotCaptureResult&)>
      callback = base::BindOnce(&LensRegionSearchController::OnCaptureCompleted,
                                weak_this_);
  in_capture_mode_ = true;
  if (use_fullscreen_capture) {
    screenshot_flow_->StartFullscreenCapture(std::move(callback));
  } else {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
    // Create user education bubble anchored to the toolbar container.
    // This is only done for non-fulllscreen capture.
    bubble_widget_ = lens::OpenLensRegionSearchInstructions(
        browser,
        base::BindOnce(&LensRegionSearchController::Close,
                       base::Unretained(this)),
        base::BindOnce(&LensRegionSearchController::Escape,
                       base::Unretained(this)));
    bubble_widget_->Show();
#endif
    screenshot_flow_->Start(std::move(callback));
  }
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
  // Close all open UI overlays and bubbles.
  CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  image_editor::ScreenshotCaptureResultCode code = result.result_code;
  if (code == image_editor::ScreenshotCaptureResultCode::USER_NAVIGATED_EXIT) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::USER_NAVIGATED_FROM_CAPTURE);
    return;
  } else if (code ==
             image_editor::ScreenshotCaptureResultCode::USER_ESCAPE_EXIT) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::USER_EXITED_CAPTURE_ESCAPE);
    return;
  }

  const gfx::Image& image = result.image;

  // If image is empty, then record UMA and close.
  if (image.IsEmpty()) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::ERROR_CAPTURING_REGION);
    return;
  }

  // Record region size related UMA histograms according to region and screen.
  RecordRegionSizeRelatedMetrics(result.screen_bounds, image.Size());

  auto* core_tab_helper = CoreTabHelper::FromWebContents(web_contents());
  if (!core_tab_helper) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::FAILED_TO_OPEN_TAB);
    return;
  }

  lens::RecordAmbientSearchQuery(entry_point_);

  if (is_google_default_search_provider_) {
    lens::EntryPoint lens_entry_point =
        lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
    switch (entry_point_) {
      case lens::AmbientSearchEntryPoint::COMPANION_REGION_SEARCH:
        lens_entry_point = lens::EntryPoint::COMPANION_REGION_SEARCH;
        break;
      case lens::AmbientSearchEntryPoint::
          LENS_OVERLAY_LOCATION_BAR_ACCESSIBILITY_FALLBACK:
        lens_entry_point = lens::EntryPoint::CHROME_LENS_OVERLAY_LOCATION_BAR;
        break;
      default:
        // The other possible values of `entry_point_` should not be possible
        // and are considered invalid.
        break;
    }
    core_tab_helper->SearchWithLens(image, lens_entry_point,
                                    force_open_in_new_tab_);
  } else {
    core_tab_helper->SearchByImage(image);
  }

  RecordCaptureResult(lens::LensRegionSearchCaptureResult::SUCCESS);
  if (web_contents() && lens::features::IsLensRegionSearchStaticPageEnabled()) {
    web_contents()->ClosePage();
  }
}

void LensRegionSearchController::WebContentsDestroyed() {
  CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

void LensRegionSearchController::OnVisibilityChanged(
    content::Visibility visibility) {
  if (in_capture_mode_ && visibility == content::Visibility::HIDDEN) {
    RecordCaptureResult(
        lens::LensRegionSearchCaptureResult::USER_NAVIGATED_FROM_CAPTURE);
    CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }
}

void LensRegionSearchController::Close() {
  CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  // Record a capture result when the instructional bubble is responsible for
  // exiting out of the capture mode.
  RecordCaptureResult(
      LensRegionSearchCaptureResult::USER_EXITED_CAPTURE_CLOSE_BUTTON);
}

void LensRegionSearchController::Escape() {
  CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  // Record a capture result when the instructional bubble is responsible for
  // exiting out of the capture mode.
  RecordCaptureResult(
      LensRegionSearchCaptureResult::USER_EXITED_CAPTURE_ESCAPE);
}

void LensRegionSearchController::CloseWithReason(
    views::Widget::ClosedReason reason) {
  in_capture_mode_ = false;
  if (bubble_widget_) {
    std::exchange(bubble_widget_, nullptr)->CloseWithReason(reason);
  }
  if (screenshot_flow_) {
    screenshot_flow_->CancelCapture();
    screenshot_flow_.reset();
  }
  if (web_contents() && lens::features::IsLensRegionSearchStaticPageEnabled()) {
    web_contents()->ClosePage();
  }
}

bool LensRegionSearchController::IsOverlayUIVisibleForTesting() {
  if (!bubble_widget_ || !screenshot_flow_)
    return false;
  return bubble_widget_->IsVisible() && screenshot_flow_->IsCaptureModeActive();
}

void LensRegionSearchController::SetEntryPointForTesting(
    lens::AmbientSearchEntryPoint entry_point) {
  entry_point_ = entry_point;
}

void LensRegionSearchController::SetWebContentsForTesting(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

}  // namespace lens
